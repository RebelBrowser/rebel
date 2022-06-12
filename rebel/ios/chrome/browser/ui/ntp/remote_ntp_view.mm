// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_view.h"

#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#include "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "net/base/apple/url_conversions.h"
#include "url/gurl.h"

#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_api_provider.h"
#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_scheme_handler.h"
#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RemoteNtpView () <WKNavigationDelegate, RemoteNtpApiObserving> {
  UrlLoadingBrowserAgent* _urlLoader;
}

@property(nonatomic) UIEdgeInsets insets;

@property(nonatomic, strong) NSURL* ntpUrl;
@property(nonatomic, strong) WKWebView* webView;

@property(nonatomic, strong) RemoteNtpViewController* remoteNtpViewController;
@property(nonatomic, strong) RemoteNtpSchemeHandler* remoteNtpSchemeHandler;
@property(nonatomic, strong) RemoteNtpApiProvider* remoteNtpApiProvider;

@end

@implementation RemoteNtpView

@synthesize ntpUrl = _ntpUrl;
@synthesize webView = _webView;
@synthesize remoteNtpViewController = _remoteNtpViewController;
@synthesize remoteNtpSchemeHandler = _remoteNtpSchemeHandler;
@synthesize remoteNtpApiProvider = _remoteNtpApiProvider;

- (instancetype)initWithFrame:(CGRect)frame
                       ntpUrl:(NSURL*)ntpUrl
                    urlLoader:(UrlLoadingBrowserAgent*)urlLoader
             remoteNtpService:(rebel::RemoteNtpService*)remoteNtpService
               viewController:(RemoteNtpViewController*)viewController
                 browserState:(ChromeBrowserState*)browserState {
  self = [super initWithFrame:frame];

  if (self) {
    _urlLoader = urlLoader;
    _ntpUrl = ntpUrl;

    self.alwaysBounceVertical = YES;
    // The bottom safe area is taken care of with layoutSubviews.
    self.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;

    WKWebViewConfiguration* configuration =
        [[WKWebViewConfiguration alloc] init];
    configuration.userContentController =
        [[WKUserContentController alloc] init];

    _remoteNtpViewController = viewController;

    _remoteNtpSchemeHandler =
        [[RemoteNtpSchemeHandler alloc] initWithBrowserState:browserState
                                            remoteNtpService:remoteNtpService
                                               configuration:configuration];

    _remoteNtpApiProvider = [[RemoteNtpApiProvider alloc]
        initWithRemoteNtpService:remoteNtpService
                  viewController:viewController
           userContentController:configuration.userContentController
                        observer:self];

    _webView = [[WKWebView alloc] initWithFrame:frame
                                  configuration:configuration];
    _webView.translatesAutoresizingMaskIntoConstraints = NO;
    _webView.autoresizingMask =
        UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    _webView.navigationDelegate = self;

    [self addSubview:_webView];

    NSURLRequest* request = [NSURLRequest requestWithURL:_ntpUrl];
    [_webView loadRequest:request];
  }

  return self;
}

#pragma mark - UIView overrides

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    bool darkModeEnabled =
        (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
    [self.remoteNtpApiProvider notifyOfDarkModeChange:darkModeEnabled];
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];

  CGFloat toolbarHeight =
      ToolbarExpandedHeight(self.traitCollection.preferredContentSizeCategory) +
      LocationBarHeight(self.traitCollection.preferredContentSizeCategory);

  if (self.insets.top != toolbarHeight) {
    UIEdgeInsets insets = self.insets;
    insets.top = toolbarHeight;

    self.webView.frame = UIEdgeInsetsInsetRect(self.frame, insets);
    self.insets = insets;
  }
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView*)webView
    didCommitNavigation:(WKNavigation*)navigation {
  [self.remoteNtpApiProvider notifyOfDidCommitNavigation];
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self.remoteNtpViewController remoteNtpLoadFailedForUrl:self.ntpUrl];
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self.remoteNtpViewController remoteNtpLoadFailedForUrl:self.ntpUrl];
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
}

- (void)webView:(WKWebView*)webView
    didReceiveServerRedirectForProvisionalNavigation:(WKNavigation*)navigation {
}

- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
  // Load any new navigation in the current tab - not in the WebView that is
  // showing the RemoteNTP.
  const GURL webViewURL = net::GURLWithNSURL(webView.URL);

  if (webViewURL.is_valid() && ![webView.URL isEqual:self.ntpUrl]) {
    _urlLoader->Load(UrlLoadParams::InCurrentTab(webViewURL));
  }

  [self.remoteNtpApiProvider notifyOfDidStartNavigation];
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)navigationAction
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)navigationResponse
                      decisionHandler:(void (^)(WKNavigationResponsePolicy))
                                          decisionHandler {
  decisionHandler(WKNavigationResponsePolicyAllow);
}

#pragma mark - RemoteNtpApiProvider overrides

- (void)loadUrl:(const GURL&)url
    transitionType:(ui::PageTransition)transitionType {
  UrlLoadParams params(UrlLoadParams::InCurrentTab(url));
  params.web_params.transition_type = transitionType;

  _urlLoader->Load(params);
}

- (void)executeJs:(const std::string&)jsStr {
  [self.webView
      evaluateJavaScript:[NSString
                             stringWithCString:jsStr.c_str()
                                      encoding:[NSString
                                                   defaultCStringEncoding]]
       completionHandler:^(id, NSError*){
       }];
}

- (bool)isDarkModeEnabled {
  return self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
}

@end
