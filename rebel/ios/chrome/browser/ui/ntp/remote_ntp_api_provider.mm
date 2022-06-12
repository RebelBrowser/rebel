// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_api_provider.h"

#include <mach/mach_host.h>
#include <mach/machine.h>

#include <sstream>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#include "ios/components/webui/web_ui_url_constants.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/components/url_formatter/rebel_constants.h"
#import "rebel/ios/chrome/browser/ntp/remote_ntp_service_ios.h"
#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_view_controller.h"
#import "rebel/ios/chrome/browser/ui/ntp/remote_ntp_view_script_message_handler.h"

namespace {

constexpr const char kApiObjectName[] = "window.rebel";

const char kRemoteNtpInitiatedApiTemplate[] = R"js(
    %s: function(...params) {
      window.webkit.messageHandlers.ntp.postMessage({
        method: "%s",
        params: [...params],
      });
    },
  )js";

const char kRemoteNtpScriptTemplate[] = R"js(
    (function() {
      if (typeof %s === 'undefined') {
        %s = {};
      }

      function is_object(item) {
        return (item && (typeof item === 'object') && !Array.isArray(item));
      }

      function deep_merge(target, ...sources) {
        if (!sources.length) return target;
        const source = sources.shift();

        if (is_object(target) && is_object(source)) {
          for (const key in source) {
            if (is_object(source[key])) {
              if (!target[key]) Object.assign(target, { [key]: {} });
              deep_merge(target[key], source[key]);
            } else {
              Object.assign(target, { [key]: source[key] });
            }
          }
        }

        return deep_merge(target, ...sources);
      }

      %s = deep_merge(%s, %s);
    })();
  )js";

const char kRemoteNtpCallbackTemplate[] = R"js(
    (function() {
      if (%s && %s && (typeof %s === 'function')) {
        %s();
        true;
      }
    })();
  )js";

}  // namespace

@interface RemoteNtpApiProvider () <RemoteNtpMessageReceiver,
                                    RemoteNtpServiceObserving> {
  rebel::RemoteNtpService* _remoteNtpService;
  std::unique_ptr<rebel::RemoteNtpServiceObserverBridge> _remoteNtpBridge;

  bool _darkModeEnabled;

  bool _ntpTilesAvailable;
  std::string _ntpTilesJson;

  std::string _platformInfoJson;

  std::string _autocompleteResult;

  std::string _lastApiJson;
}

@property(nonatomic, weak) RemoteNtpViewController* viewController;

@property(nonatomic, weak) id<ApplicationCommands, BrowserCoordinatorCommands>
    dispatcher;

@property(nonatomic, weak) id<RemoteNtpApiObserving> observer;

@end

@implementation RemoteNtpApiProvider

@synthesize viewController = _viewController;
@synthesize dispatcher = _dispatcher;
@synthesize observer = _observer;

- (id)initWithRemoteNtpService:
          (rebel::RemoteNtpService* _Nonnull)remoteNtpService
                viewController:(RemoteNtpViewController*)viewController
         userContentController:(WKUserContentController*)userContentController
                      observer:(id<RemoteNtpApiObserving>)observer {
  self = [super init];

  if (self) {
    _remoteNtpService = remoteNtpService;
    _viewController = viewController;
    _dispatcher = viewController.dispatcher;
    _observer = observer;

    _darkModeEnabled = [self.observer isDarkModeEnabled];
    _remoteNtpService->SetDarkModeEnabled(_darkModeEnabled);

    _remoteNtpBridge =
        std::make_unique<rebel::RemoteNtpServiceObserverBridge>(self);
    _remoteNtpService->AddObserver(_remoteNtpBridge.get());

    _ntpTilesAvailable = false;
    _ntpTilesJson = "[]";
    _autocompleteResult = "{}";

    [self setPlatformInfo];

    [self initializeApiObject:userContentController];
  }

  return self;
}

- (void)dealloc {
  if (_remoteNtpService && _remoteNtpBridge) {
    _remoteNtpService->RemoveObserver(_remoteNtpBridge.get());
  }
}

- (void)notifyOfDidCommitNavigation {
  if (_remoteNtpService) {
    _remoteNtpService->OnNewTabPageOpened();
  }
}

- (void)notifyOfDidStartNavigation {
  _remoteNtpBridge->DidStartNavigation();
}

- (void)notifyOfDarkModeChange:(bool)darkModeEnabled {
  if (_remoteNtpService) {
    _remoteNtpService->SetDarkModeEnabled(darkModeEnabled);
  }
}

- (void)setPlatformInfo {
  // This is how chrome://version determines browser architecture.
  int browserArch = (sizeof(void*) == 8) ? 64 : 32;
  int systemArch = [RemoteNtpApiProvider is64BitDevice] ? 64 : 32;

  base::Value::Dict platformInfoJson;
  platformInfoJson.Set("platform", version_info::GetOSType());
  platformInfoJson.Set("version", version_info::GetVersionNumber());
  platformInfoJson.Set("browserArch", browserArch);
  platformInfoJson.Set("systemArch", systemArch);

  JSONStringValueSerializer(&_platformInfoJson).Serialize(platformInfoJson);
}

+ (BOOL)is64BitDevice {
#if __LP64__
  return YES;
#else
  static BOOL kIs64BitDevice = NO;
  static dispatch_once_t kOnceToken;

  dispatch_once(&kOnceToken, ^{
    struct host_basic_info info;
    mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;

    kern_return_t result =
        host_info(mach_host_self(), HOST_BASIC_INFO,
                  reinterpret_cast<host_info_t>(&info), &count);

    if (result == KERN_SUCCESS) {
      cpu_type_t arch = info.cpu_type & CPU_ARCH_MASK;
      kIs64BitDevice = (arch == CPU_ARCH_ABI64) || (arch == CPU_ARCH_ABI64_32);
    }
  });

  return kIs64BitDevice;
#endif
}

- (void)initializeApiObject:(WKUserContentController*)userContentController {
  [self serializeApiObject];

  @try {
    [userContentController
        addScriptMessageHandler:[[RemoteNtpViewScriptMessageHandler alloc]
                                    initWithReceiver:self]
                           name:@"ntp"];
  } @catch (NSException* exception) {
    NSLog(@"Failed to add RemoteNTP script message handler: %@",
          exception.reason);
  }

  NSString* ntpIosApiScriptString =
      [NSString stringWithCString:_lastApiJson.c_str()
                         encoding:[NSString defaultCStringEncoding]];

  WKUserScript* ntpIosApiScript = [[WKUserScript alloc]
        initWithSource:ntpIosApiScriptString
         injectionTime:WKUserScriptInjectionTimeAtDocumentStart
      forMainFrameOnly:NO];

  [userContentController addUserScript:ntpIosApiScript];
}

- (void)updateApiObjectForObserver {
  [self serializeApiObject];
  [self.observer executeJs:_lastApiJson];
}

- (void)serializeApiObject {
  std::stringstream jsStream;
  jsStream << std::boolalpha;

  jsStream << "{ ";
  jsStream << "ntpTilesAvailable: " << _ntpTilesAvailable << ", ";
  jsStream << "ntpTiles: " << _ntpTilesJson << ", ";
  jsStream << "platformInfo: " << _platformInfoJson << ", ";
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "setNtpVersion", "setNtpVersion");
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "getNtpVersion", "getNtpVersion");
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "addCustomTile", "addCustomTile");
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "removeCustomTile", "removeCustomTile");
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "editCustomTile", "editCustomTile");
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "loadInternalUrl", "loadInternalUrl");
  jsStream << "search: {";
  jsStream << "autocompleteResult: " << _autocompleteResult << ", ";
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "queryAutocomplete", "queryAutocomplete");
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "stopAutocomplete", "stopAutocomplete");
  jsStream << base::StringPrintf(kRemoteNtpInitiatedApiTemplate,
                                 "openAutocompleteMatch",
                                 "openAutocompleteMatch");
  jsStream << "},";
  jsStream << "theme: {";
  jsStream << "darkModeEnabled: " << _darkModeEnabled << ", ";
  jsStream << "},";
  jsStream << "}";

  _lastApiJson = base::StringPrintf(kRemoteNtpScriptTemplate, kApiObjectName,
                                    kApiObjectName, kApiObjectName,
                                    kApiObjectName, jsStream.str().c_str());
}

- (void)triggerNtpCallback:(const char*)method {
  const std::string api = base::StringPrintf("%s.%s", kApiObjectName, method);

  const std::string trigger =
      base::StringPrintf(kRemoteNtpCallbackTemplate, kApiObjectName,
                         api.c_str(), api.c_str(), api.c_str());

  [self.observer executeJs:trigger];
}

- (void)triggerOnNtpTilesChanged {
  [self triggerNtpCallback:"onNtpTilesChanged"];
}

- (void)triggerOnAutocompleteResultChanged {
  [self triggerNtpCallback:"search.onAutocompleteResultChanged"];
}

- (void)triggerOnThemeChanged {
  [self triggerNtpCallback:"theme.onThemeChanged"];
}

#pragma mark - RemoteNtpMessageReceiver

- (void)onAddCustomTile:(std::string)tileUrl
              tileTitle:(std::u16string)tileTitle {
  const GURL url(std::move(tileUrl));

  if (_remoteNtpService) {
    _remoteNtpService->AddCustomTile(url, std::move(tileTitle));
  }
}

- (void)onRemoveCustomTile:(std::string)tileUrl {
  const GURL url(std::move(tileUrl));

  if (_remoteNtpService) {
    _remoteNtpService->RemoveCustomTile(url);
  }
}

- (void)onEditCustomTile:(std::string)oldTileUrl
              newTileUrl:(std::string)newTileUrl
            newTileTitle:(std::u16string)newTileTitle {
  const GURL oldUrl(std::move(oldTileUrl));
  const GURL newUrl(std::move(newTileUrl));

  if (_remoteNtpService) {
    _remoteNtpService->EditCustomTile(oldUrl, newUrl, std::move(newTileTitle));
  }
}

- (void)onLoadInternalUrl:(std::string)url {
  // The following logic based on:
  // rebel/chrome/renderer/ntp/remote_ntp_extension.cc
  // rebel/chrome/android/java/src/org/chromium/chrome/browser/app/RebelActivity.java
  const GURL validatedUrl(url);

  if (!validatedUrl.is_valid() || !rebel::SchemeIsRebelOrChrome(validatedUrl)) {
    return;
  }

  base::StringPiece host = validatedUrl.host_piece();

  if (host == "settings") {
    [self.dispatcher showSettingsFromViewController:self.viewController];
  } else if (host == "history") {
    [self.dispatcher showHistory];
  } else if (host == "bookmarks") {
    [self.dispatcher showBookmarksManager];
  } else if (host == "downloads") {
    // TODO: Implement if needed. This is implemented for Android but not used.
  } else {
    [self.observer loadUrl:validatedUrl
            transitionType:ui::PAGE_TRANSITION_LINK];
  }
}

- (void)onQueryAutocomplete:(std::u16string)input
    preventInlineAutocomplete:(BOOL)preventInlineAutocomplete {
  _remoteNtpBridge->QueryAutocomplete(_remoteNtpService, input,
                                      preventInlineAutocomplete);
}

- (void)onStopAutocomplete {
  _remoteNtpBridge->StopAutocomplete();
}

- (void)onOpenAutocompleteMatch:(uint32_t)index url:(std::string)url {
  const GURL matchUrl(std::move(url));

  GURL destinationUrl;
  ui::PageTransition transitionType;

  if (!_remoteNtpBridge->MatchSelected(index, matchUrl, destinationUrl,
                                       transitionType)) {
    return;
  }

  [self.observer loadUrl:destinationUrl transitionType:transitionType];
}

#pragma mark - RemoteNtpServiceObserving

- (void)onNtpTilesChanged:(const rebel::RemoteNtpTileList&)ntpTiles {
  base::Value::List tilesJson;

  for (const auto& tile : ntpTiles) {
    base::Value::Dict tileJson;
    tileJson.Set("url", tile->url);
    tileJson.Set("favicon_url", tile->favicon_url);
    tileJson.Set("title", tile->title);

    tilesJson.Append(std::move(tileJson));
  }

  JSONStringValueSerializer(&_ntpTilesJson).Serialize(tilesJson);
  _ntpTilesAvailable = true;

  [self updateApiObjectForObserver];
  [self triggerOnNtpTilesChanged];
}

- (void)onAutocompleteResultChanged:
    (rebel::mojom::AutocompleteResultPtr)result {
  // Helper to convert a list of match classifications to JSON.
  auto class_value = [](const auto& classes) {
    base::Value::List classesJson;

    for (const auto& clss : classes) {
      base::Value::Dict classJson;
      classJson.Set("offset", static_cast<int>(clss->offset));
      classJson.Set("style", clss->style);

      classesJson.Append(std::move(classJson));
    }

    return classesJson;
  };

  base::Value::List matchesJson;

  for (const rebel::mojom::AutocompleteMatchPtr& match : result->matches) {
    base::Value::Dict matchJson;

    matchJson.Set("contents", match->contents);
    matchJson.Set("contentsClass", class_value(match->contents_class));

    matchJson.Set("description", match->description);
    matchJson.Set("descriptionClass", class_value(match->description_class));

    matchJson.Set("destinationUrl", match->destination_url);

    matchJson.Set("type", match->type);
    matchJson.Set("isSearchType", match->is_search_type);

    matchJson.Set("fillIntoEdit", match->fill_into_edit);
    matchJson.Set("inlineAutocompletion", match->inline_autocompletion);
    matchJson.Set("allowedToBeDefaultMatch",
                  match->allowed_to_be_default_match);

    matchesJson.Append(std::move(matchJson));
  }

  base::Value::Dict resultJson;
  resultJson.Set("input", result->input);
  resultJson.Set("matches", std::move(matchesJson));

  JSONStringValueSerializer(&_autocompleteResult).Serialize(resultJson);

  [self updateApiObjectForObserver];
  [self triggerOnAutocompleteResultChanged];
}

- (void)onThemeChanged:(rebel::mojom::RemoteNtpThemePtr)theme {
  _darkModeEnabled = theme->dark_mode_enabled;

  [self updateApiObjectForObserver];
  [self triggerOnThemeChanged];
}

@end
