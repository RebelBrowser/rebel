// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_view_controller.h"

#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "net/base/apple/url_conversions.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"
#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RemoteNtpViewController () {
  UrlLoadingBrowserAgent* _urlLoader;
  rebel::RemoteNtpService* _remoteNtpService;
}

@property(nonatomic, strong) NSURL* ntpUrl;
@property(nonatomic, strong) NSURL* ntpOfflineUrl;

@end

@implementation RemoteNtpViewController

@synthesize ntpUrl = _ntpUrl;
@synthesize ntpOfflineUrl = _ntpOfflineUrl;

- (id)initWithUrlLoader:(UrlLoadingBrowserAgent*)urlLoader
       remoteNtpService:(rebel::RemoteNtpService*)remoteNtpService {
  self = [super init];

  if (self) {
    _ntpUrl = net::NSURLWithGURL(rebel::GetRemoteNtpUrl());
    _ntpOfflineUrl = net::NSURLWithGURL(GURL(rebel::kRemoteNtpOfflineUrl));

    _urlLoader = urlLoader;
    _remoteNtpService = remoteNtpService;
  }

  return self;
}

- (void)remoteNtpLoadFailedForUrl:(NSURL*)ntpUrl {
  if (![ntpUrl isEqual:self.ntpOfflineUrl]) {
    [self.remoteNtpView removeFromSuperview];
    [self loadRemoteNtpWithUrl:self.ntpOfflineUrl];
  }
}

- (void)viewDidLoad {
  [self loadRemoteNtpWithUrl:self.ntpUrl];
}

- (void)loadRemoteNtpWithUrl:(NSURL*)ntpUrl {
  self.remoteNtpView = [[RemoteNtpView alloc] initWithFrame:self.view.bounds
                                                     ntpUrl:ntpUrl
                                                  urlLoader:_urlLoader
                                           remoteNtpService:_remoteNtpService
                                             viewController:self
                                               browserState:self.browserState];

  [self.remoteNtpView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                          UIViewAutoresizingFlexibleWidth];

  [self.view addSubview:self.remoteNtpView];
}

- (void)dealloc {
  [_remoteNtpView setDelegate:nil];
}

@end
