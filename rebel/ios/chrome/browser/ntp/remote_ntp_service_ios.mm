// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ntp/remote_ntp_service_ios.h"

#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#include "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/ntp_tiles/model/ios_most_visited_sites_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/thread/web_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

namespace rebel {

RemoteNtpServiceIos::RemoteNtpServiceIos(ChromeBrowserState* browser_state)
    : RemoteNtpService(browser_state->GetStatePath(),
                       browser_state->GetPrefs()),
      browser_state_(browser_state) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!web::WebThread::CurrentlyOn(web::WebThread::UI)) {
    return;
  }

  InitializeService(
      IOSMostVisitedSitesFactory::NewForBrowserState(browser_state),
      GetApplicationContext()->GetSharedURLLoaderFactory());
}

std::unique_ptr<AutocompleteController>
RemoteNtpServiceIos::CreateAutocompleteController() const {
  return std::make_unique<AutocompleteController>(
      std::make_unique<AutocompleteProviderClientImpl>(browser_state_),
      AutocompleteClassifier::DefaultOmniboxProviders());
}

RemoteNtpServiceObserverBridge::RemoteNtpServiceObserverBridge(
    id<RemoteNtpServiceObserving> observer)
    : remote_ntp_search_provider_(this), observer_(observer) {}

void RemoteNtpServiceObserverBridge::QueryAutocomplete(
    rebel::RemoteNtpService* remote_ntp_service,
    const std::u16string& input,
    bool prevent_inline_autocomplete) {
  remote_ntp_search_provider_.QueryAutocomplete(
      remote_ntp_service, input, prevent_inline_autocomplete,
      AutocompleteSchemeClassifierImpl());
}

void RemoteNtpServiceObserverBridge::StopAutocomplete() {
  remote_ntp_search_provider_.StopAutocomplete();
}

bool RemoteNtpServiceObserverBridge::MatchSelected(
    uint32_t index,
    const GURL& url,
    GURL& destination_url,
    ui::PageTransition& transition_type) {
  return remote_ntp_search_provider_.MatchSelected(index, url, destination_url,
                                                   transition_type);
}

void RemoteNtpServiceObserverBridge::DidStartNavigation() {
  remote_ntp_search_provider_.DidStartNavigation();
}

void RemoteNtpServiceObserverBridge::OnAutocompleteResultChanged(
    rebel::mojom::AutocompleteResultPtr result) {
  [observer_ onAutocompleteResultChanged:std::move(result)];
}

void RemoteNtpServiceObserverBridge::OnNtpTilesChanged(
    const rebel::RemoteNtpTileList& ntp_tiles) {
  [observer_ onNtpTilesChanged:ntp_tiles];
}

void RemoteNtpServiceObserverBridge::OnThemeChanged(
    rebel::mojom::RemoteNtpThemePtr theme) {
  [observer_ onThemeChanged:std::move(theme)];
}

}  // namespace rebel
