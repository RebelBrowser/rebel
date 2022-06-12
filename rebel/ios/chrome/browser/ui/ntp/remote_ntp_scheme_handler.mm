// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_scheme_handler.h"

#include <map>
#include <string>

#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/core/large_icon_service.h"
#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/favicon/model/large_icon_cache.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "net/base/apple/url_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/chrome/browser/ntp/remote_ntp_offline_resources.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/common/ntp/remote_ntp_icon_util.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"
#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_tile_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Matches chrome::kChromeSearchScheme
NSString* const kChromeSearchScheme = @"chrome-search";

// Matches chrome::kChromeUINewTabIconHost
NSString* const kChromeUINewTabIconHost = @"ntpicon";

// Matches NtpIconSource::GetMimeType
NSString* const kNtpIconMimeType = @"image/png";

// Size of the favicon returned by the provider for the NTP tiles.
const CGFloat kNtpTilesFaviconSize = 48;

// Size below which the provider returns a colored tile instead of an image.
const CGFloat kNtpTilesFaviconMinimalSize = 16;

}  // namespace

@interface RemoteNtpSchemeHandler () {
  rebel::RemoteNtpService* _remoteNtpService;
}

// FaviconAttributesProvider to fetch favicons for the NTP tiles.
@property(nonatomic, nullable, strong, readwrite)
    FaviconAttributesProvider* ntpTilesAttributesProvider;

@end

@implementation RemoteNtpSchemeHandler {
  // Maintain pointers to outstanding tasks to prevent handling duplicate
  // completions for the same task. See CRWWebUISchemeHandler.
  std::map<id<WKURLSchemeTask>, NSURL*> _pendingTaskMap;
}

@synthesize ntpTilesAttributesProvider = _ntpTilesAttributesProvider;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                    remoteNtpService:(rebel::RemoteNtpService*)remoteNtpService
                       configuration:(WKWebViewConfiguration*)configuration {
  self = [super init];

  if (self) {
    _remoteNtpService = remoteNtpService;

    favicon::LargeIconService* largeIconService =
        IOSChromeLargeIconServiceFactory::GetForBrowserState(browserState);
    LargeIconCache* largeIconCache =
        IOSChromeLargeIconCacheFactory::GetForBrowserState(browserState);

    _ntpTilesAttributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kNtpTilesFaviconSize
             minFaviconSize:kNtpTilesFaviconMinimalSize
           largeIconService:largeIconService];
    _ntpTilesAttributesProvider.cache = largeIconCache;

    [configuration setURLSchemeHandler:self forURLScheme:kChromeSearchScheme];
  }

  return self;
}

#pragma mark - WKURLSchemeHandler overrides

- (void)webView:(WKWebView*)webView
    startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  NSURL* url = urlSchemeTask.request.URL;

  if ([url.scheme compare:kChromeSearchScheme] == NSOrderedSame) {
    [self onChromeSearchRequest:url urlSchemeTask:urlSchemeTask];
  } else {
    NOTREACHED() << "Received unregistered scheme: " << net::GURLWithNSURL(url);
    [self failTask:urlSchemeTask withError:NSURLErrorUnsupportedURL];
  }
}

- (void)webView:(WKWebView*)webView
    stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  auto result = _pendingTaskMap.find(urlSchemeTask);
  if (result != _pendingTaskMap.end()) {
    _pendingTaskMap.erase(result);
  }
}

#pragma mark - chrome-search scheme handlers

- (void)onChromeSearchRequest:(NSURL*)url
                urlSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  static NSString* kRemoteNtpOfflineHost = nil;
  static dispatch_once_t onceToken;

  dispatch_once(&onceToken, ^{
    kRemoteNtpOfflineHost =
        base::SysUTF8ToNSString(rebel::kRemoteNtpOfflineHost);
  });

  if ([url.host compare:kChromeUINewTabIconHost] == NSOrderedSame) {
    [self onNtpIconRequest:url urlSchemeTask:urlSchemeTask];
  } else if ([url.host compare:kRemoteNtpOfflineHost] == NSOrderedSame) {
    [self onRemoteNtpOfflineRequest:url urlSchemeTask:urlSchemeTask];
  } else {
    [self failTask:urlSchemeTask withError:NSURLErrorUnsupportedURL];
  }
}

- (void)onNtpIconRequest:(NSURL*)url
           urlSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  __weak RemoteNtpSchemeHandler* weakSelf = self;

  void (^completion)(RemoteNtpTileView*) = ^(RemoteNtpTileView* tile) {
    RemoteNtpSchemeHandler* strongSelf = weakSelf;
    if (!strongSelf ||
        (strongSelf.map->find(urlSchemeTask) == strongSelf.map->end())) {
      return;
    }

    UIImage* image = CaptureViewWithOption(tile, 0.0, kAfterScreenUpdate);
    NSData* data = UIImagePNGRepresentation(image);

    [strongSelf completeTask:url
               urlSchemeTask:urlSchemeTask
                        data:data
                    mimeType:kNtpIconMimeType];
  };

  bool queued = [RemoteNtpTileView fetchNtpTile:url
                                       provider:self.ntpTilesAttributesProvider
                                     completion:completion];

  if (queued) {
    _pendingTaskMap.insert(std::make_pair(urlSchemeTask, url));
  } else {
    [weakSelf failTask:urlSchemeTask withError:NSURLErrorBadURL];
  }
}

- (void)onRemoteNtpOfflineRequest:(NSURL*)url
                    urlSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  const std::string file_path = base::SysNSStringToUTF8([url path]).substr(1);

  if (file_path == rebel::kRemoteNtpIconPath) {
    [self onRemoteNtpIconRequest:url urlSchemeTask:urlSchemeTask];
    return;
  }

  float scale = 1.0f;
  auto scale_factor = ui::GetSupportedResourceScaleFactor(scale);

  // Note: This mimics |RemoteNtpSource::StartDataRequest|.
  for (auto const& offline_resource : rebel::kRemoteNtpOfflineResources) {
    if (file_path == offline_resource.file_path) {
      scoped_refptr<base::RefCountedMemory> resource(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
              offline_resource.identifier, scale_factor));

      NSData* data = [NSData dataWithBytes:resource->front()
                                    length:resource->size()];

      NSString* mimeType = base::SysUTF8ToNSString(offline_resource.mime_type);

      [self completeTask:url
           urlSchemeTask:urlSchemeTask
                    data:data
                mimeType:mimeType];

      return;
    }
  }

  [self failTask:urlSchemeTask withError:NSURLErrorFileDoesNotExist];
}

- (void)onRemoteNtpIconRequest:(NSURL*)url
                 urlSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  if (!_remoteNtpService || !_remoteNtpService->icon_storage()) {
    return;
  }

  const rebel::RemoteNtpIcon parsed =
      rebel::ParseIconFromURL(net::GURLWithNSURL(url));
  if (!parsed.url.is_valid()) {
    return;
  }

  __weak RemoteNtpSchemeHandler* weakSelf = self;

  void (^completion)(SkBitmap) = ^(SkBitmap icon) {
    RemoteNtpSchemeHandler* strongSelf = weakSelf;
    if (!strongSelf ||
        (strongSelf.map->find(urlSchemeTask) == strongSelf.map->end())) {
      return;
    } else if (icon.empty()) {
      [self removeUrl:url];
      return;
    }

    UIImage* image = gfx::Image::CreateFrom1xBitmap(icon).ToUIImage();
    NSData* data = UIImagePNGRepresentation(image);

    [strongSelf completeTask:url
               urlSchemeTask:urlSchemeTask
                        data:data
                    mimeType:kNtpIconMimeType];
  };

  _remoteNtpService->icon_storage()->GetIconForOrigin(
      parsed.url.GetWithEmptyPath(), parsed.icon_size,
      base::BindOnce(completion));

  _pendingTaskMap.insert(std::make_pair(urlSchemeTask, url));
}

#pragma mark - Private

// Complete a WKURLSchemeTask with the given response data.
- (void)completeTask:(NSURL*)url
       urlSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask
                data:(NSData*)data
            mimeType:(NSString*)mimeType {
  NSURLResponse* response = [[NSURLResponse alloc] initWithURL:url
                                                      MIMEType:mimeType
                                         expectedContentLength:0
                                              textEncodingName:nil];

  [urlSchemeTask didReceiveResponse:response];
  [urlSchemeTask didReceiveData:data];
  [urlSchemeTask didFinish];

  [self removeUrl:url];
}

// Fails a WKURLSchemeTask with the given error code.
- (void)failTask:(id<WKURLSchemeTask>)urlSchemeTask
       withError:(NSInteger)errorCode {
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:errorCode
                      userInfo:@{
                        NSURLErrorKey : urlSchemeTask.request.URL,
                        NSURLErrorFailingURLStringErrorKey :
                            urlSchemeTask.request.URL.absoluteString
                      }];

  [urlSchemeTask didFailWithError:error];
}

// Returns a pointer to the |_pendingTaskMap| member for strongSelf.
- (std::map<id<WKURLSchemeTask>, NSURL*>*)map {
  return &_pendingTaskMap;
}

// Removes |url| from map of active fetchers.
- (void)removeUrl:(NSURL*)url {
  auto it = std::find_if(
      _pendingTaskMap.begin(), _pendingTaskMap.end(),
      [url](const std::pair<const id<WKURLSchemeTask>, NSURL*>& entry) {
        return [url isEqual:entry.second];
      });

  if (it != _pendingTaskMap.end()) {
    _pendingTaskMap.erase(it);
  }
}

@end
