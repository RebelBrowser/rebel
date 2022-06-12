// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IOS_H_
#define REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IOS_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/page_transition_types.h"

#include "rebel/chrome/browser/ntp/remote_ntp_search_provider.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

class AutocompleteController;
class ChromeBrowserState;
class GURL;

// Observes RemoteNtpService from Objective-C. To use as a
// RemoteNtpService::Observer, wrap in a RemoteNtpServiceObserverBridge.
@protocol RemoteNtpServiceObserving <NSObject>

- (void)onNtpTilesChanged:(const rebel::RemoteNtpTileList&)ntpTiles;

- (void)onAutocompleteResultChanged:(rebel::mojom::AutocompleteResultPtr)result;

- (void)onThemeChanged:(rebel::mojom::RemoteNtpThemePtr)theme;

@end

namespace rebel {

// Implementation of RemoteNtpService for iOS devices.
class RemoteNtpServiceIos : public RemoteNtpService {
 public:
  explicit RemoteNtpServiceIos(ChromeBrowserState* browser_state);

  // Overridden from rebel::RemoteNtpService:
  std::unique_ptr<AutocompleteController> CreateAutocompleteController()
      const override;

 private:
  RemoteNtpServiceIos(const RemoteNtpServiceIos&) = delete;
  RemoteNtpServiceIos& operator=(const RemoteNtpServiceIos&) = delete;

  raw_ptr<ChromeBrowserState> browser_state_;
};

// Observer for the RemoteNtpService and RemoteNtpSearchProvider that translates
// the C++ observer callbacks to Objective-C calls.
class RemoteNtpServiceObserverBridge
    : public rebel::RemoteNtpSearchProvider::Delegate,
      public rebel::RemoteNtpService::Observer {
 public:
  RemoteNtpServiceObserverBridge(id<RemoteNtpServiceObserving> observer);

  void QueryAutocomplete(rebel::RemoteNtpService* remote_ntp_service,
                         const std::u16string& input,
                         bool prevent_inline_autocomplete);
  void StopAutocomplete();
  bool MatchSelected(uint32_t index,
                     const GURL& url,
                     GURL& destination_url,
                     ui::PageTransition& transition_type);
  void DidStartNavigation();

 private:
  RemoteNtpServiceObserverBridge(const RemoteNtpServiceObserverBridge&) =
      delete;
  RemoteNtpServiceObserverBridge& operator=(
      const RemoteNtpServiceObserverBridge&) = delete;

  // Overriden from rebel::RemoteNtpSearchProvider::Delegate:
  void OnAutocompleteResultChanged(
      rebel::mojom::AutocompleteResultPtr result) override;

  // Overridden from rebel::RemoteNtpService::Observer:
  void OnNtpTilesChanged(const rebel::RemoteNtpTileList& ntp_tiles) override;
  void OnThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) override;

  rebel::RemoteNtpSearchProvider remote_ntp_search_provider_;

  __weak id<RemoteNtpServiceObserving> observer_ = nil;
};

}  // namespace rebel

#endif  // REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IOS_H_
