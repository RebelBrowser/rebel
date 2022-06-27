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
#include "chrome/browser/ui/singleton_tabs.h"
#endif

#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "rebel/chrome/browser/android/ntp/remote_ntp_bridge.h"
#else
#include "rebel/chrome/browser/ntp/remote_ntp_theme_provider.h"
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

#if !BUILDFLAG(IS_ANDROID)
  remote_ntp_theme_provider_ =
      std::make_unique<rebel::RemoteNtpThemeProvider>(this, profile());
#endif
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
      web_contents()->GetMainFrame()->GetProcess();

  if (process_host && remote_ntp_service_) {
    int process_id = process_host->GetID();

    if (remote_ntp_service_->IsRemoteNtpProcess(process_id)) {
      remote_ntp_service_->OnNewTabPageOpened();
    }
  }
}

void RemoteNtpTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
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
      web_contents()->GetMainFrame()->GetProcess();

  if (process_host && remote_ntp_service_ &&
      remote_ntp_service_->IsRemoteNtpProcess(process_host->GetID())) {
    remote_ntp_theme_provider_->RevertColor(web_contents());
  }
#endif
}

void RemoteNtpTabHelper::WebContentsDestroyed() {
#if !BUILDFLAG(IS_ANDROID)
  content::RenderProcessHost* process_host =
      web_contents()->GetMainFrame()->GetProcess();

  if (process_host && remote_ntp_service_ &&
      remote_ntp_service_->IsRemoteNtpProcess(process_host->GetID())) {
    remote_ntp_theme_provider_->RevertColor(web_contents());
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
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  if (browser) {
    NavigateParams params(GetSingletonTabNavigateParams(browser, url));
    params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;

    ShowSingletonTabOverwritingNTP(browser, &params);
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
                             transition_type, false));
#endif
}

void RemoteNtpTabHelper::OnLoadBackgroundCollections() {
  if (remote_ntp_service_) {
    remote_ntp_service_->LoadBackgroundCollections();
  }
}

void RemoteNtpTabHelper::OnLoadBackgroundImages(
    const std::string& collection_id) {
  if (remote_ntp_service_) {
    remote_ntp_service_->LoadBackgroundImages(collection_id);
  }
}

void RemoteNtpTabHelper::OnSetBackgroundImage(
    const std::string& collection_id,
    rebel::mojom::BackgroundImagePtr image) {
  if (remote_ntp_service_) {
    remote_ntp_service_->SetBackgroundImage(collection_id, std::move(image));
  }
}

void RemoteNtpTabHelper::OnSelectLocalBackgroundImage() {
#if !BUILDFLAG(IS_ANDROID)
  remote_ntp_theme_provider_->SelectLocalBackgroundImage(web_contents());
#endif
}

void RemoteNtpTabHelper::OnPreviewColor(SkColor color) {
#if !BUILDFLAG(IS_ANDROID)
  remote_ntp_theme_provider_->PreviewColor(web_contents(), color);
#endif
}

void RemoteNtpTabHelper::OnRevertColor() {
#if !BUILDFLAG(IS_ANDROID)
  remote_ntp_theme_provider_->RevertColor(nullptr);
#endif
}

void RemoteNtpTabHelper::OnCommitColor() {
#if !BUILDFLAG(IS_ANDROID)
  remote_ntp_theme_provider_->CommitColor();
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

void RemoteNtpTabHelper::OnLocalBackgroundImageSelected() {
  remote_ntp_router_.SendLocalBackgroundImageSelected();
}

void RemoteNtpTabHelper::OnNtpTilesChanged(
    const rebel::RemoteNtpTileList& ntp_tiles) {
  remote_ntp_router_.SendNtpTilesChanged(ntp_tiles);
}

void RemoteNtpTabHelper::OnBackgroundCollectionsChanged(
    const rebel::RemoteNtpBackgroundCollectionList& collections) {
  remote_ntp_router_.SendBackgroundCollectionsChanged(collections);
}

void RemoteNtpTabHelper::OnBackgroundImagesChanged(
    const rebel::RemoteNtpBackgroundImageMap& images) {
  remote_ntp_router_.SendBackgroundImagesChanged(images);
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
