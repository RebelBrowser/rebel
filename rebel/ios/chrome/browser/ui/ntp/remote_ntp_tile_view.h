// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TILE_VIEW_H_
#define REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TILE_VIEW_H_

#import <UIKit/UIKit.h>

@class FaviconAttributes;
@class FaviconAttributesProvider;

@interface RemoteNtpTileView : UIView

// Fetch a favicon for the RemoteNTP. When available, captures the result in a
// |RemoteNtpTileView| and triggers |completion| with that view.
+ (bool)fetchNtpTile:(NSURL*)url
            provider:(FaviconAttributesProvider*)provider
          completion:(void (^)(RemoteNtpTileView*))completion;

// A UIView for use on the RemoteNTP. Configures a standalone view with a
// favicon (or monogram fallback) at the center of a larger, transparent frame.
- (instancetype)initWithAttributes:(FaviconAttributes*)attributes
                          tileSize:(CGFloat)tileSize;

@end

#endif  // REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TILE_VIEW_H_
