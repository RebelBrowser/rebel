// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_SCHEME_HANDLER_H_
#define REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_SCHEME_HANDLER_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

class ChromeBrowserState;

namespace rebel {
class RemoteNtpService;
}  // namespace rebel

// Class to handle URLs used by the RemoteNTP which are not handled by Chromium
// WebUI handlers on iOS. For example, on blink-based platforms, Chromium sets
// handlers for a variety of hosts under the chrome-search:// scheme. On iOS,
// these do not exist; so this class defines handlers to mimic them.
@interface RemoteNtpSchemeHandler : NSObject <WKURLSchemeHandler>

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                    remoteNtpService:(rebel::RemoteNtpService*)remoteNtpService
                       configuration:(WKWebViewConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_SCHEME_HANDLER_H_
