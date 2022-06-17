// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_view_script_message_handler.h"

#include <ostream>

#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"

@interface RemoteNtpViewScriptMessageHandler ()

@property(nonatomic, weak) id<RemoteNtpMessageReceiver> receiver;

@end

@implementation RemoteNtpViewScriptMessageHandler

@synthesize receiver = _receiver;

- (id)initWithReceiver:(id<RemoteNtpMessageReceiver>)receiver {
  self = [super init];

  if (self) {
    _receiver = receiver;
  }

  return self;
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  NSString* method = message.body[@"method"];
  NSArray* params = message.body[@"params"];

  // We receive page events we do not care about, e.g. a "loadComplete" event.
  if ((method == nil) || (params == nil)) {
    return;
  }

  // addCustomTile(url, title)
  if ([method compare:@"addCustomTile"] == NSOrderedSame) {
    if ([params count] == 2U) {
      std::string url = [params[0] UTF8String];
      std::u16string title = base::SysNSStringToUTF16(params[1]);

      [self.receiver onAddCustomTile:std::move(url) tileTitle:std::move(title)];
    } else {
      NOTREACHED()
          << "addCustomTile must be called with |url| and |title| params";
    }
  }

  // removeCustomTile(url)
  if ([method compare:@"removeCustomTile"] == NSOrderedSame) {
    if ([params count] == 1U) {
      std::string url = [params[0] UTF8String];

      [self.receiver onRemoveCustomTile:std::move(url)];
    } else {
      NOTREACHED() << "removeCustomTile must be called with |url| param";
    }
  }

  // editCustomTile(oldUrl, newUrl, newTitle)
  if ([method compare:@"editCustomTile"] == NSOrderedSame) {
    if ([params count] == 3U) {
      std::string oldUrl = [params[0] UTF8String];
      std::string newUrl = [params[1] UTF8String];
      std::u16string newTitle = base::SysNSStringToUTF16(params[2]);

      [self.receiver onEditCustomTile:std::move(oldUrl)
                           newTileUrl:std::move(newUrl)
                         newTileTitle:std::move(newTitle)];
    } else {
      NOTREACHED() << "editCustomTile must be called with |oldUrl|, |newUrl| "
                      "and |newTitle| params";
    }
  }

  // loadInternalUrl(url)
  if ([method compare:@"loadInternalUrl"] == NSOrderedSame) {
    if ([params count] == 1U) {
      std::string url = [params[0] UTF8String];

      if (!url.empty()) {
        [self.receiver onLoadInternalUrl:std::move(url)];
      }
    } else {
      NOTREACHED() << "loadInternalUrl must be called with |url| param";
    }
  }

  // queryAutocomplete(input, preventInlineAutocomplete)
  else if ([method compare:@"queryAutocomplete"] == NSOrderedSame) {
    if ([params count] == 2U) {
      std::u16string input = base::SysNSStringToUTF16(params[0]);
      BOOL preventInlineAutocomplete = [params[1] boolValue];

      [self.receiver onQueryAutocomplete:std::move(input)
               preventInlineAutocomplete:preventInlineAutocomplete];
    } else {
      NOTREACHED() << "queryAutocomplete must be called with |input| and "
                      "|preventInlineAutocomplete| params";
    }
  }

  // stopAutocomplete()
  else if ([method compare:@"stopAutocomplete"] == NSOrderedSame) {
    [self.receiver onStopAutocomplete];
  }

  // openAutocompleteMatch(index, url, ...)
  else if ([method compare:@"openAutocompleteMatch"] == NSOrderedSame) {
    // There are several more params but we can ignore them for iOS. They are
    // all booleans for e.g. if a middle mouse button was pressed.
    if ([params count] >= 2U) {
      uint32_t index = [params[0] intValue];
      std::string url = [params[1] UTF8String];

      [self.receiver onOpenAutocompleteMatch:index url:std::move(url)];
    } else {
      NOTREACHED() << "openAutocompleteMatch must be called with |index| and "
                      "|url| params";
    }
  }
}

@end
