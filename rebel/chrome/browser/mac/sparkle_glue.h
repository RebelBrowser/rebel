// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_MAC_SPARKLE_GLUE_H_
#define REBEL_CHROME_BROWSER_MAC_SPARKLE_GLUE_H_

#if defined(__OBJC__)

#import <AppKit/AppKit.h>

#include <memory>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

class Profile;

@class SparkleObserver;

using SparkleUpdaterCallbackList =
    base::RepeatingCallbackList<void(VersionUpdater::Status, NSString*)>;

// macOS Sparkle implementation of version update functionality, used by the
// WebUI About/Help page and installed version poller.
class VersionUpdaterSparkle : public VersionUpdater {
 public:
  ~VersionUpdaterSparkle() override;

  // VersionUpdater implementation.
  void CheckForUpdate(StatusCallback status_callback,
                      PromoteCallback promote_callback) override;
  void PromoteUpdater() override {}

 private:
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterSparkle(Profile* profile);

  VersionUpdaterSparkle(const VersionUpdaterSparkle&) = delete;
  VersionUpdaterSparkle& operator=(const VersionUpdaterSparkle&) = delete;

  // Sparkle's statuses processor method.
  void UpdateStatus(Status status, NSString* error_string);
  void UpdateStatusOnUIThread(Status status, NSString* error_string);

  // Callback used to communicate update status to the client.
  StatusCallback status_callback_;

  // Callback used to show or hide the promote UI elements.
  PromoteCallback promote_callback_;

  // The observer that will receive Sparkle status updates.
  base::CallbackListSubscription sparkle_subscription_;

  base::WeakPtrFactory<VersionUpdaterSparkle> weak_ptr_factory_;
};

#endif  // __OBJC__

// Functions that may be accessed from non-Objective-C C/C++ code.
namespace rebel {

// Attempt to relaunch browser and install updates if any.
void RelaunchBrowserUsingSparkle();

// Initializes Sparkle Framework with default values.
void InitializeSparkleFramework();

// True if Sparkle is enabled.
bool SparkleEnabled();

// The version of the application currently downloaded and ready to be installed
// (with NEARLY_UPDATED status).
std::u16string CurrentlyDownloadedVersion();

// Check if current application has an update which is ready to be installed.
bool ApplicationIsNearlyUpdated();

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_MAC_SPARKLE_GLUE_H_
