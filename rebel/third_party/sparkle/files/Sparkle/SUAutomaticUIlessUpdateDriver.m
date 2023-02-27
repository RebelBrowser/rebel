// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "SUAutomaticUIlessUpdateDriver.h"

#import "SUAppcastItem.h"
#import "SUApplicationInfo.h"
#import "SUConstants.h"
#import "SUErrors.h"
#import "SUHost.h"
#import "SULocalizations.h"
#import "SUUpdaterDelegate.h"
#import "SUUpdaterPrivate.h"

@interface SUAutomaticUIlessUpdateDriver ()

@property(assign) BOOL willUpdateOnTermination;
@property(assign) BOOL isTerminating;

@end

@implementation SUAutomaticUIlessUpdateDriver

@synthesize willUpdateOnTermination;
@synthesize isTerminating;

- (void)unarchiverDidFinish:(id)__unused ua {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(applicationWillTerminate:)
             name:NSApplicationWillTerminateNotification
           object:nil];
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserver:self
         selector:@selector(systemWillPowerOff:)
             name:NSWorkspaceWillPowerOffNotification
           object:nil];

  // Sudden termination is available on 10.6+
  NSProcessInfo* processInfo = [NSProcessInfo processInfo];
  [processInfo disableSuddenTermination];

  self.willUpdateOnTermination = YES;

  id<SUUpdaterPrivate> updater = self.updater;
  id<SUUpdaterDelegate> updaterDelegate = [updater delegate];
  if ([updaterDelegate
          respondsToSelector:@selector
          (updater:willInstallUpdateOnQuit:immediateInstallationInvocation:)]) {
    BOOL relaunch = YES;
    BOOL showUI = NO;
    NSInvocation* invocation = [NSInvocation
        invocationWithMethodSignature:
            [[self class] instanceMethodSignatureForSelector:@selector
                          (installWithToolAndRelaunch:
                              displayingUserInterface:)]];
    [invocation setSelector:@selector(installWithToolAndRelaunch:
                                         displayingUserInterface:)];
    [invocation setArgument:&relaunch atIndex:2];
    [invocation setArgument:&showUI atIndex:3];
    [invocation setTarget:self];

    [updaterDelegate updater:self.updater
                willInstallUpdateOnQuit:self.updateItem
        immediateInstallationInvocation:invocation];
  }

  if ([updaterDelegate respondsToSelector:@selector
                       (updater:didReachNearlyUpdateStateForItem:)]) {
    [updaterDelegate updater:self.updater
        didReachNearlyUpdateStateForItem:self.updateItem];
  }

  NSDictionary* userInfo =
      (self.updateItem != nil)
          ? @{SUUpdaterAppcastItemNotificationKey : self.updateItem}
          : nil;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:SUUpdaterDidReachNearlyUpdatedStateNotification
                    object:self.updater
                  userInfo:userInfo];
}

- (void)stopUpdatingOnTermination {
  if (self.willUpdateOnTermination) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSApplicationWillTerminateNotification
                object:nil];
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        removeObserver:self
                  name:NSWorkspaceWillPowerOffNotification
                object:nil];

    NSProcessInfo* processInfo = [NSProcessInfo processInfo];
    [processInfo enableSuddenTermination];

    self.willUpdateOnTermination = NO;

    id<SUUpdaterPrivate> updater = self.updater;
    id<SUUpdaterDelegate> updaterDelegate = [updater delegate];
    if ([updaterDelegate respondsToSelector:@selector(updater:
                                                didCancelInstallUpdateOnQuit:)])
      [updaterDelegate updater:self.updater
          didCancelInstallUpdateOnQuit:self.updateItem];
  }
}

- (void)dealloc {
  [self stopUpdatingOnTermination];
}

- (void)abortUpdate {
  self.isTerminating = NO;
  [self stopUpdatingOnTermination];
  [super abortUpdate];
}

- (void)installWithToolAndRelaunch:(BOOL)relaunch
           displayingUserInterface:(BOOL)showUI {
  if (relaunch) {
    [self stopUpdatingOnTermination];
  }

  [super installWithToolAndRelaunch:relaunch displayingUserInterface:showUI];
}

- (void)systemWillPowerOff:(NSNotification*)__unused note {
  [self
      abortUpdateWithError:
          [NSError errorWithDomain:SUSparkleErrorDomain
                              code:SUSystemPowerOffError
                          userInfo:@{
                            NSLocalizedDescriptionKey : SULocalizedString(
                                @"The update will not be installed because the "
                                @"user requested for the system to power off",
                                nil)
                          }]];
}

- (void)applicationWillTerminate:(NSNotification*)__unused note {
  // We don't want to terminate the app if the user or someone else initiated a
  // termination Use a property instead of passing an argument to
  // installWithToolAndRelaunch: because we give the delegate an invocation to
  // our install methods and this code was added later :|
  self.isTerminating = YES;

  [self installWithToolAndRelaunch:NO];
}

- (void)terminateApp {
  if (!self.isTerminating) {
    [super terminateApp];
  }
}

@end
