// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TAB_HELPER_H_
#define REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkColor.h"

#include "rebel/chrome/browser/ntp/remote_ntp_search_provider.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
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

// This is the browser-side, per-tab implementation of the RemoteNTP API.
class RemoteNtpTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RemoteNtpTabHelper>,
      public rebel::RemoteNtpRouter::Delegate,
      public rebel::RemoteNtpSearchProvider::Delegate,
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
  void OnShowOrHideCustomizeMenu() override;
  void OnUpdateWiFiStatus() override;

  // Overriden from rebel::RemoteNtpSearchProvider::Delegate:
  void OnAutocompleteResultChanged(
      rebel::mojom::AutocompleteResultPtr result) override;

  // Overridden from rebel::RemoteNtpService::Observer:
  void OnNtpTilesChanged(const rebel::RemoteNtpTileList& ntp_tiles) override;
  void OnThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) override;
  void OnWiFiStatusChanged(
      const rebel::RemoteNtpWiFiStatusList& status) override;

  Profile* profile() const;

  rebel::RemoteNtpRouter remote_ntp_router_;
  raw_ptr<rebel::RemoteNtpService> remote_ntp_service_;
  rebel::RemoteNtpSearchProvider remote_ntp_search_provider_;

#if BUILDFLAG(IS_ANDROID)
  raw_ptr<rebel::RemoteNtpBridge> remote_ntp_bridge_;
#endif

  friend class content::WebContentsUserData<RemoteNtpTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_TAB_HELPER_H_
