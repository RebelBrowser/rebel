// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_tile_view.h"

#include <algorithm>

#include "components/favicon_base/favicon_util.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "net/base/apple/url_conversions.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/common/ntp/remote_ntp_icon_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RemoteNtpTileView ()

@property(nonatomic, strong) FaviconView* faviconView;

@end

@implementation RemoteNtpTileView

+ (bool)fetchNtpTile:(NSURL*)url
            provider:(FaviconAttributesProvider*)provider
          completion:(void (^)(RemoteNtpTileView*))completion {
  const rebel::RemoteNtpIcon parsed =
      rebel::ParseIconFromURL(net::GURLWithNSURL(url));
  if (!parsed.url.is_valid()) {
    return false;
  }

  void (^iconAvailable)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
    // If the attributes are invalid, or if there isn't a real favicon available
    // and the RemoteNTP requested no fallback, show the default favicon.
    if (!attributes ||
        (!attributes.faviconImage && !parsed.show_fallback_monogram)) {
      attributes = [FaviconAttributes attributesWithDefaultImage];
    }

    RemoteNtpTileView* tile =
        [[RemoteNtpTileView alloc] initWithAttributes:attributes
                                             tileSize:parsed.icon_size];

    completion(tile);
  };

  [provider fetchFaviconAttributesForURL:parsed.url completion:iconAvailable];
  return true;
}

- (instancetype)initWithAttributes:(FaviconAttributes*)attributes
                          tileSize:(CGFloat)tileSize {
  self = [super initWithFrame:CGRectMake(0, 0, tileSize, tileSize)];

  if (self) {
    _faviconView = [[FaviconView alloc] init];
    _faviconView.font = [UIFont systemFontOfSize:22];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    [_faviconView configureWithAttributes:attributes];

    [self addSubview:_faviconView];

    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.heightAnchor constraintEqualToConstant:tileSize * 2 / 3],
      [_faviconView.widthAnchor
          constraintEqualToAnchor:_faviconView.heightAnchor],
    ]];

    AddSameCenterConstraints(_faviconView, self);
  }

  return self;
}

@end
