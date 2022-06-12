// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_SCRIPT_MESSAGE_HANDLER_H_
#define REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_SCRIPT_MESSAGE_HANDLER_H_

#import <WebKit/WebKit.h>

#include <string>

// RemoteNtpViewScriptMessageHandler calls its receiver in response to messages
// received from the RemoteNTP.
@protocol RemoteNtpMessageReceiver <NSObject>

// Called when the user wants to add a custom tile.
- (void)onAddCustomTile:(std::string)tileUrl
              tileTitle:(std::u16string)tileTitle;

// Called when the user wants to remove a custom tile.
- (void)onRemoveCustomTile:(std::string)tileUrl;

// Called when the user wants to update a custom tile.
- (void)onEditCustomTile:(std::string)oldTileUrl
              newTileUrl:(std::string)newTileUrl
            newTileTitle:(std::u16string)newTileTitle;

// Called when the RemoteNTP wants to open a chrome:// URL.
- (void)onLoadInternalUrl:(std::string)url;

// Called when the RemoteNTP wants to obtain autocomplete results.
- (void)onQueryAutocomplete:(std::u16string)input
    preventInlineAutocomplete:(BOOL)preventInlineAutocomplete;

// Called when the RemoteNTP wants to cancel an autocomplete query.
- (void)onStopAutocomplete;

// Called when the RemoteNTP wants to navigate to an autocomplete match.
- (void)onOpenAutocompleteMatch:(uint32_t)index url:(std::string)url;

@end

// RemoteNtpViewScriptMessageHandler is responsible for receiving and sending
// messages between the browser and the RemoteNTP.
@interface RemoteNtpViewScriptMessageHandler : NSObject <WKScriptMessageHandler>

- (id)initWithReceiver:(id<RemoteNtpMessageReceiver>)receiver;

@end

#endif  // REBEL_IOS_CHROME_BROWSER_UI_NTP_REMOTE_NTP_VIEW_SCRIPT_MESSAGE_HANDLER_H_
