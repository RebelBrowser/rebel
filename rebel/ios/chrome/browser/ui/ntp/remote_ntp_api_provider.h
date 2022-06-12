// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_API_PROVIDER_H_
#define REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_API_PROVIDER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include <string>

#include "ui/base/page_transition_types.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"

class GURL;

namespace rebel {
class RemoteNtpService;
}  // namespace rebel

@class RemoteNtpViewController;

@protocol RemoteNtpApiObserving <NSObject>

- (void)loadUrl:(const GURL&)url
    transitionType:(ui::PageTransition)transitionType;

- (void)executeJs:(const std::string&)jsStr;

- (bool)isDarkModeEnabled;

@end

@interface RemoteNtpApiProvider : NSObject
- (instancetype)
    initWithRemoteNtpService:(rebel::RemoteNtpService*)remoteNtpService
              viewController:(RemoteNtpViewController*)viewController
       userContentController:(WKUserContentController*)userContentController
                    observer:(id<RemoteNtpApiObserving>)observer;

- (void)notifyOfDidCommitNavigation;

- (void)notifyOfDidStartNavigation;

- (void)notifyOfDarkModeChange:(bool)darkModeEnabled;

@end

#endif  // REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_API_PROVIDER_H_
