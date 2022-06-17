// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_H_
#define REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

class ChromeBrowserState;
class UrlLoadingBrowserAgent;

namespace rebel {
class RemoteNtpService;
}  // namespace rebel

@class RemoteNtpViewController;

@interface RemoteNtpView : UIScrollView

- (instancetype)initWithFrame:(CGRect)frame
                       ntpUrl:(NSURL*)ntpUrl
                    urlLoader:(UrlLoadingBrowserAgent*)urlLoader
             remoteNtpService:(rebel::RemoteNtpService*)remoteNtpService
               viewController:(RemoteNtpViewController*)viewController
                 browserState:(ChromeBrowserState*)browserState;

@end

#endif  // REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_H_
