// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/startup/client_registration.h"

#include "ios/chrome/browser/net/chrome_cookie_store_ios_client.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#include "ios/net/cookies/cookie_store_ios_client.h"
#import "ios/web/public/web_client.h"

#include "build/branding_buildflags.h"  // Needed for REBEL_BROWSER.
#if BUILDFLAG(REBEL_BROWSER)
#include "rebel/ios/chrome/browser/web/rebel_web_client.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ClientRegistration

+ (void)registerClients {
#if BUILDFLAG(REBEL_BROWSER)
  web::SetWebClient(new rebel::RebelWebClient());
#else
  web::SetWebClient(new ChromeWebClient());
#endif
  // Register CookieStoreIOSClient, This is used to provide CookieStoreIOSClient
  // users with WEB::IO task runner.
  net::SetCookieStoreIOSClient(new ChromeCookieStoreIOSClient());
}

@end
