// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TAB_HELPER_H_
#define REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkColor.h"

#include "rebel/chrome/browser/ntp/remote_ntp_search_provider.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/browser/ntp/remote_ntp_theme_delegate.h"
#include "rebel/chrome/browser/ui/ntp/remote_ntp_router.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

namespace content {
struct LoadCommittedDetails;
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

class GURL;
class Profile;

namespace rebel {

class RemoteNtpBridge;
class RemoteNtpThemeProvider;

// This is the browser-side, per-tab implementation of the RemoteNTP API.
class RemoteNtpTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RemoteNtpTabHelper>,
      public rebel::RemoteNtpRouter::Delegate,
      public rebel::RemoteNtpSearchProvider::Delegate,
      public rebel::RemoteNtpThemeDelegate,
      public rebel::RemoteNtpService::Observer {
 public:
  ~RemoteNtpTabHelper() override;

  static void BindRemoteNtpConnector(
      mojo::PendingAssociatedReceiver<rebel::mojom::RemoteNtpConnector>
          receiver,
      content::RenderFrameHost* render_frame_host);

#if BUILDFLAG(IS_ANDROID)
  void SetRemoteNtpBridge(rebel::RemoteNtpBridge* remote_ntp_bridge) {
    remote_ntp_bridge_ = remote_ntp_bridge;
  }
#endif

 private:
  explicit RemoteNtpTabHelper(content::WebContents* web_contents);

  RemoteNtpTabHelper(const RemoteNtpTabHelper&) = delete;
  RemoteNtpTabHelper& operator=(const RemoteNtpTabHelper&) = delete;

  // Overridden from contents::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // Overridden from rebel::RemoteNtpRouter::Delegate:
  void OnAddCustomTile(const GURL& tile_url,
                       const std::u16string& tile_title) override;
  void OnRemoveCustomTile(const GURL& tile_url) override;
  void OnEditCustomTile(const GURL& old_tile_url,
                        const GURL& new_tile_url,
                        const std::u16string& new_tile_title) override;
  void OnLoadInternalUrl(const GURL& url) override;
  void OnQueryAutocomplete(const std::u16string& input,
                           bool prevent_inline_autocomplete) override;
  void OnStopAutocomplete() override;
  void OnOpenAutocompleteMatch(uint32_t index,
                               const GURL& url,
                               bool middle_button,
                               bool alt_key,
                               bool ctrl_key,
                               bool meta_key,
                               bool shift_key) override;
  void OnLoadBackgroundCollections() override;
  void OnLoadBackgroundImages(const std::string& collection_id) override;
  void OnSetBackgroundImage(const std::string& collection_id,
                            rebel::mojom::BackgroundImagePtr image) override;
  void OnSelectLocalBackgroundImage() override;
  void OnPreviewColor(SkColor color) override;
  void OnRevertColor() override;
  void OnCommitColor() override;

  // Overriden from rebel::RemoteNtpSearchProvider::Delegate:
  void OnAutocompleteResultChanged(
      rebel::mojom::AutocompleteResultPtr result) override;

  // Overriden from rebel::RemoteNtpThemeDelegate:
  void OnLocalBackgroundImageSelected() override;

  // Overridden from rebel::RemoteNtpService::Observer:
  void OnNtpTilesChanged(const rebel::RemoteNtpTileList& ntp_tiles) override;
  void OnBackgroundCollectionsChanged(
      const rebel::RemoteNtpBackgroundCollectionList& collections) override;
  void OnBackgroundImagesChanged(
      const rebel::RemoteNtpBackgroundImageMap& images) override;
  void OnThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) override;

  Profile* profile() const;

  rebel::RemoteNtpRouter remote_ntp_router_;
  rebel::RemoteNtpService* remote_ntp_service_;
  rebel::RemoteNtpSearchProvider remote_ntp_search_provider_;

#if BUILDFLAG(IS_ANDROID)
  rebel::RemoteNtpBridge* remote_ntp_bridge_;
#else
  std::unique_ptr<rebel::RemoteNtpThemeProvider> remote_ntp_theme_provider_;
#endif

  friend class content::WebContentsUserData<RemoteNtpTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TAB_HELPER_H_
