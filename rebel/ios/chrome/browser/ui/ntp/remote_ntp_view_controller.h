// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_CONTROLLER_H_
#define REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

class ChromeBrowserState;
class UrlLoadingBrowserAgent;

namespace rebel {
class RemoteNtpService;
}  // namespace rebel

@protocol ApplicationCommands;
@protocol BrowserCoordinatorCommands;

@interface RemoteNtpViewController : UIViewController

@property(nonatomic, strong) UIScrollView* remoteNtpView;

@property(nonatomic, weak) id<ApplicationCommands, BrowserCoordinatorCommands>
    dispatcher;

@property(nonatomic, assign) ChromeBrowserState* browserState;

// Init with the given loader object. |loader| may be nil, but isn't retained so
// it must outlive this controller.
- (id)initWithUrlLoader:(UrlLoadingBrowserAgent*)urlLoader
       remoteNtpService:(rebel::RemoteNtpService*)remoteNtpService;

- (void)remoteNtpLoadFailedForUrl:(NSURL*)ntpUrl;

@end

#endif  // REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_CONTROLLER_H_
