// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ui/ntp/remote_ntp_tab_helper.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "ui/base/window_open_disposition_utils.h"
#endif

#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "rebel/chrome/browser/android/ntp/remote_ntp_bridge.h"
#endif

namespace rebel {

RemoteNtpTabHelper::RemoteNtpTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<RemoteNtpTabHelper>(*web_contents),
      remote_ntp_router_(web_contents, this),
      remote_ntp_service_(RemoteNtpServiceFactory::GetForProfile(profile())),
      remote_ntp_search_provider_(this) {
  if (remote_ntp_service_) {
    remote_ntp_service_->AddObserver(this);
  }
}

RemoteNtpTabHelper::~RemoteNtpTabHelper() {
  if (remote_ntp_service_) {
    remote_ntp_service_->RemoveObserver(this);
  }
}

void RemoteNtpTabHelper::BindRemoteNtpConnector(
    mojo::PendingAssociatedReceiver<rebel::mojom::RemoteNtpConnector> receiver,
    content::RenderFrameHost* render_frame_host) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }

  auto* tab_helper = RemoteNtpTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    return;
  }

  tab_helper->remote_ntp_router_.BindRemoteNtpConnector(std::move(receiver),
                                                        render_frame_host);
}

void RemoteNtpTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  content::RenderProcessHost* process_host =
      web_contents()->GetPrimaryMainFrame()->GetProcess();

  if (process_host && remote_ntp_service_) {
#if !BUILDFLAG(IS_ANDROID)
    auto* customize_chrome_tab_helper =
        CustomizeChromeTabHelper::FromWebContents(web_contents());
    customize_chrome_tab_helper->CreateAndRegisterEntry();
    customize_chrome_tab_helper->SetCallback(base::DoNothing());
#endif

    int process_id = process_host->GetID();

    if (remote_ntp_service_->IsRemoteNtpProcess(process_id) ||
        remote_ntp_service_->IsRemoteNtpAPIProcess(process_id)) {
      remote_ntp_service_->OnNewTabPageOpened();
    }
  }
}

void RemoteNtpTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_handle->GetReloadType() != content::ReloadType::NONE) {
    remote_ntp_search_provider_.DidStartNavigation();
  }

  if (navigation_handle->IsSameDocument()) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  content::RenderProcessHost* process_host =
      web_contents()->GetPrimaryMainFrame()->GetProcess();

  if (process_host && remote_ntp_service_ &&
      remote_ntp_service_->IsRemoteNtpProcess(process_host->GetID())) {
    auto* customize_chrome_tab_helper =
        CustomizeChromeTabHelper::FromWebContents(web_contents());
    customize_chrome_tab_helper->DeregisterEntry();
  }
#endif
}

void RemoteNtpTabHelper::WebContentsDestroyed() {
#if !BUILDFLAG(IS_ANDROID)
  content::RenderProcessHost* process_host =
      web_contents()->GetPrimaryMainFrame()->GetProcess();

  if (process_host && remote_ntp_service_ &&
      remote_ntp_service_->IsRemoteNtpProcess(process_host->GetID())) {
    auto* customize_chrome_tab_helper =
        CustomizeChromeTabHelper::FromWebContents(web_contents());
    customize_chrome_tab_helper->DeregisterEntry();
  }
#endif
}

void RemoteNtpTabHelper::OnAddCustomTile(const GURL& tile_url,
                                         const std::u16string& tile_title) {
  if (remote_ntp_service_) {
    remote_ntp_service_->AddCustomTile(tile_url, tile_title);
  }
}

void RemoteNtpTabHelper::OnRemoveCustomTile(const GURL& tile_url) {
  if (remote_ntp_service_) {
    remote_ntp_service_->RemoveCustomTile(tile_url);
  }
}

void RemoteNtpTabHelper::OnEditCustomTile(
    const GURL& old_tile_url,
    const GURL& new_tile_url,
    const std::u16string& new_tile_title) {
  if (remote_ntp_service_) {
    remote_ntp_service_->EditCustomTile(old_tile_url, new_tile_url,
                                        new_tile_title);
  }
}

void RemoteNtpTabHelper::OnLoadInternalUrl(const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  if (remote_ntp_bridge_) {
    remote_ntp_bridge_->LoadInternalUrl(url);
  }
#else
  Browser* browser = chrome::FindBrowserWithTab(web_contents());

  if (browser) {
    NavigateParams params(GetSingletonTabNavigateParams(browser, url));
    params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;

    ShowSingletonTabOverwritingNTP(&params);
  }
#endif
}

void RemoteNtpTabHelper::OnQueryAutocomplete(const std::u16string& input,
                                             bool prevent_inline_autocomplete) {
  remote_ntp_search_provider_.QueryAutocomplete(
      remote_ntp_service_, input, prevent_inline_autocomplete,
      ChromeAutocompleteSchemeClassifier(profile()));
}

void RemoteNtpTabHelper::OnStopAutocomplete() {
  remote_ntp_search_provider_.StopAutocomplete();
}

void RemoteNtpTabHelper::OnOpenAutocompleteMatch(uint32_t index,
                                                 const GURL& url,
                                                 bool middle_button,
                                                 bool alt_key,
                                                 bool ctrl_key,
                                                 bool meta_key,
                                                 bool shift_key) {
  GURL destination_url;
  ui::PageTransition transition_type;

  if (!remote_ntp_search_provider_.MatchSelected(index, url, destination_url,
                                                 transition_type)) {
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  if (remote_ntp_bridge_) {
    remote_ntp_bridge_->LoadAutocompleteMatchUrl(destination_url,
                                                 transition_type);
  }
#else
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      middle_button, alt_key, ctrl_key, meta_key, shift_key);

  web_contents()->OpenURL(
      content::OpenURLParams(destination_url, content::Referrer(), disposition,
                             transition_type, false),
      {});
#endif
}

void RemoteNtpTabHelper::OnShowOrHideCustomizeMenu() {
#if !BUILDFLAG(IS_ANDROID)
  if (!remote_ntp_service_) {
    return;
  }

  auto* customize_chrome_tab_helper =
      CustomizeChromeTabHelper::FromWebContents(web_contents());

  auto visible = !customize_chrome_tab_helper->IsCustomizeChromeEntryShowing();

  customize_chrome_tab_helper->SetCustomizeChromeSidePanelVisible(
      visible, CustomizeChromeSection::kUnspecified);
#endif
}

void RemoteNtpTabHelper::OnUpdateWiFiStatus() {
#if BUILDFLAG(IS_ANDROID)
  if (remote_ntp_bridge_) {
    remote_ntp_bridge_->UpdateWiFiStatus();
  }
#else
  if (remote_ntp_service_) {
    remote_ntp_service_->UpdateWiFiStatus();
  }
#endif
}

void RemoteNtpTabHelper::OnAutocompleteResultChanged(
    rebel::mojom::AutocompleteResultPtr result) {
  remote_ntp_router_.SendAutocompleteResultChanged(std::move(result));
}

void RemoteNtpTabHelper::OnNtpTilesChanged(
    const rebel::RemoteNtpTileList& ntp_tiles) {
  remote_ntp_router_.SendNtpTilesChanged(ntp_tiles);
}

void RemoteNtpTabHelper::OnThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) {
  remote_ntp_router_.SendThemeChanged(std::move(theme));
}

void RemoteNtpTabHelper::OnWiFiStatusChanged(
    const rebel::RemoteNtpWiFiStatusList& status) {
  remote_ntp_router_.SendWiFiStatusChanged(std::move(status));
}

Profile* RemoteNtpTabHelper::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RemoteNtpTabHelper);

}  // namespace rebel
