// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/mac/sparkle_glue.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

#include "rebel/third_party/sparkle/src/Sparkle/Sparkle.h"

namespace {

class VersionUpdaterForTests : public VersionUpdater {
 public:
  void CheckForUpdate(StatusCallback callback, PromoteCallback) override {
    callback.Run(DISABLED, 0, false, false, std::string(), 0, std::u16string());
  }

  void PromoteUpdater() override {}

 protected:
  friend class VersionUpdater;

  VersionUpdaterForTests() {}
  ~VersionUpdaterForTests() override {}

 private:
  VersionUpdaterForTests(const VersionUpdaterForTests&) = delete;
  VersionUpdaterForTests& operator=(const VersionUpdaterForTests&) = delete;
};

}  // namespace

// A version comparator to allow downgrading versions when the user has decided
// to change channels.
@interface ChannelRespectingVersionComparator : NSObject <SUVersionComparison>
@end

@implementation ChannelRespectingVersionComparator

- (instancetype)init {
  return [super init];
}

- (NSComparisonResult)compareVersion:(NSString*)versionA
                           toVersion:(NSString*)versionB {
  auto* comparator = [SUStandardVersionComparator defaultComparator];
  return [comparator compareVersion:versionA toVersion:versionB];
}

@end

// SparkleObserver is a simple notification observer for Sparkle status.
@interface SparkleObserver : NSObject <SUUpdaterDelegate> {
  SparkleUpdaterCallbackList status_callbacks_;
  SEQUENCE_CHECKER(sequence_checker_);
}

@end  // @interface SparkleObserver

@implementation SparkleObserver

- (id)init {
  if ((self = [super init])) {
    DETACH_FROM_SEQUENCE(sequence_checker_);

    [self registerAsSparkleObserver];
    [SUUpdater sharedUpdater].delegate = self;
  }

  return self;
}

- (base::CallbackListSubscription)registerStatusCallback:
    (SparkleUpdaterCallbackList::CallbackType)callback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return status_callbacks_.Add(std::move(callback));
}

- (void)dealloc {
  [SUUpdater sharedUpdater].delegate = nil;
  [self unregisterAsSparkleObserver];
  [super dealloc];
}

- (void)registerAsSparkleObserver {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center
      addObserver:self
         selector:@selector(handleUpdaterDidFinishLoadingAppCastNotification:)
             name:SUUpdaterDidFinishLoadingAppCastNotification
           object:nil];
  [center addObserver:self
             selector:@selector(handleUpdaterDidFindValidUpdateNotification:)
                 name:SUUpdaterDidFindValidUpdateNotification
               object:nil];
  [center addObserver:self
             selector:@selector
             (handleUpdaterDidReachNearlyUpdatedStateNotification:)
                 name:SUUpdaterDidReachNearlyUpdatedStateNotification
               object:nil];
  [center addObserver:self
             selector:@selector(handleUpdaterDidNotFindUpdateNotification:)
                 name:SUUpdaterDidNotFindUpdateNotification
               object:nil];
}

- (void)unregisterAsSparkleObserver {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

// Received reply from server with a list of updates
- (void)handleUpdaterDidFinishLoadingAppCastNotification:
    (NSNotification*)notification {
  [self updateStatus:VersionUpdater::CHECKING error_string:nil];
}

// In the list of updates there is a valid update
- (void)handleUpdaterDidFindValidUpdateNotification:
    (NSNotification*)notification {
  [self updateStatus:VersionUpdater::UPDATING error_string:nil];
}

// There is downloaded and unarchived version, waiting for application quit
- (void)handleUpdaterDidReachNearlyUpdatedStateNotification:
    (NSNotification*)notification {
  [self updateStatus:VersionUpdater::NEARLY_UPDATED error_string:nil];
}

// In the list of updates there are no new version available
- (void)handleUpdaterDidNotFindUpdateNotification:
    (NSNotification*)notification {
  [self updateStatus:VersionUpdater::UPDATED error_string:nil];
}

// Delegated method. Error handler for the Sparkle messages.
- (void)updater:(SUUpdater*)updater didAbortWithError:(NSError*)error {
  if (error.code == SUNoUpdateError) {
    // Handled by notifications
    return;
  }
  [self updateStatus:VersionUpdater::FAILED
        error_string:[error.localizedDescription copy]];
}

- (void)updateStatus:(VersionUpdater::Status)status
        error_string:(NSString*)error_string {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_callbacks_.Notify(status, error_string);
}

@end  // @implementation SparkleObserver

VersionUpdater* VersionUpdater::Create(content::WebContents*) {
  if (rebel::SparkleEnabled()) {
    return new VersionUpdaterSparkle;
  }

  return new VersionUpdaterForTests;
}

VersionUpdaterSparkle::VersionUpdaterSparkle() : weak_ptr_factory_(this) {
  // Ensure the Sparkle observer is only created once because there is only a
  // single Sparkle instance. We do not want to reset Sparkle's delegate, etc.
  static SparkleObserver* sparkle_observer = nil;
  static dispatch_once_t token;

  dispatch_once(&token, ^{
    sparkle_observer = [[SparkleObserver alloc] init];
  });

  sparkle_subscription_ = [sparkle_observer
      registerStatusCallback:base::BindRepeating(
                                 &VersionUpdaterSparkle::UpdateStatus,
                                 weak_ptr_factory_.GetWeakPtr())];
}

VersionUpdaterSparkle::~VersionUpdaterSparkle() = default;

void VersionUpdaterSparkle::CheckForUpdate(StatusCallback status_callback,
                                           PromoteCallback promote_callback) {
  // Copy the callbacks, we will re-use this for the remaining lifetime
  // of this object.
  status_callback_ = status_callback;
  promote_callback_ = promote_callback;

  SUUpdater* updater = [SUUpdater sharedUpdater];
  if (updater) {
    if (updater.isNearlyUpdated) {
      // When updater already has update, don't interrupt it by new check,
      // instead suggest user to "Relaunch" browser.
      UpdateStatus(NEARLY_UPDATED, nil);
    } else {
      // Set initial status to CHECKING, callback will advance that status as
      // progress of updates continue.
      UpdateStatus(CHECKING, nil);
      // Launch a new update check, even if one was already completed, because
      // a new update may be available or a new update may have been installed
      // in the background since the last time the Help page was displayed.
      [updater checkForUpdatesInBackground];
    }
  } else {
    // There is no glue, or the application is on a read-only filesystem.
    // Updates and promotions are impossible.
    status_callback_.Run(DISABLED, 0, false, false, std::string(), 0,
                         std::u16string());
  }
}

void VersionUpdaterSparkle::UpdateStatus(Status status,
                                         NSString* error_string) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&VersionUpdaterSparkle::UpdateStatusOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(), status, error_string));
}

void VersionUpdaterSparkle::UpdateStatusOnUIThread(Status status,
                                                   NSString* error_string) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status_callback_.is_null()) {
    return;
  }

  std::string error_messages = base::SysNSStringToUTF8(
      base::mac::ObjCCastStrict<NSString>(error_string));
  std::u16string message;

  // If we have an error to display, include the detail messages
  // below the error in a <pre> block. Don't bother displaying detail messages
  // on a success/in-progress/indeterminate status.
  if (!error_messages.empty()) {
    VLOG(1) << "Update error messages: " << error_messages;

    if (status == FAILED) {
      if (!message.empty()) {
        message += base::UTF8ToUTF16(std::string("<br/><br/>"));
      }

      message += l10n_util::GetStringUTF16(IDS_UPGRADE_ERROR_DETAILS);
      message += base::UTF8ToUTF16(std::string("<br/><pre>"));
      message += base::UTF8ToUTF16(base::EscapeForHTML(error_messages));
      message += base::UTF8ToUTF16(std::string("</pre>"));
    }
  }

  status_callback_.Run(status, 0, false, false, std::string(), 0, message);
}

namespace rebel {

void RelaunchBrowserUsingSparkle() {
  // Tell Sparkle to restart if possible.
  SUUpdater* updater = [SUUpdater sharedUpdater];
  if (updater) {
    [updater forceInstallAndRelaunch];
  }
}

void InitializeSparkleFramework() {
  SUUpdater* updater = [SUUpdater sharedUpdater];
  if (updater) {
    updater.automaticallyChecksForUpdates = YES;
    updater.automaticallyDownloadsUpdates = YES;
    updater.automaticallyUpdatesWithoutUI = YES;
  }
}

bool SparkleEnabled() {
#if BUILDFLAG(REBEL_SPARKLE_ENABLED)
  if (base::mac::AmIBundled()) {
    return [SUUpdater sharedUpdater] != nil;
  }
#endif

  return false;
}

std::u16string CurrentlyDownloadedVersion() {
  SUUpdater* updater = [SUUpdater sharedUpdater];
  if (!updater) {
    return std::u16string();
  }

  NSString* version = updater.nearlyUpdatedVersionString;
  if (!version) {
    return std::u16string();
  }

  return base::SysNSStringToUTF16(base::mac::ObjCCastStrict<NSString>(version));
}

bool ApplicationIsNearlyUpdated() {
  SUUpdater* updater = [SUUpdater sharedUpdater];
  if (!updater) {
    return false;
  }

  return updater.isNearlyUpdated;
}

}  // namespace rebel
