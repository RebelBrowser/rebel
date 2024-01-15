// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_H_
#define REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/renderer/instant_restricted_id_cache.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

namespace content {
class RenderFrame;
}  // namespace content

class GURL;

namespace rebel {

// The renderer-side implementation of the RemoteNTP API.
class RemoteNtp : public content::RenderFrameObserver,
                  public content::RenderFrameObserverTracker<RemoteNtp>,
                  public rebel::mojom::RemoteNtpClient {
 public:
  using RemoteNtpTileCache =
      InstantRestrictedIDCache<rebel::mojom::RemoteNtpTile>;

  explicit RemoteNtp(content::RenderFrame* render_frame);
  ~RemoteNtp() override;

  bool AreNtpTilesAvailable() const;
  void GetNtpTiles(RemoteNtpTileCache::ItemIDVector* tiles) const;

  void AddCustomTile(const GURL& tile_url,
                     const std::u16string& tile_title) const;
  void RemoveCustomTile(const GURL& tile_url) const;
  void EditCustomTile(const GURL& old_tile_url,
                      const GURL& new_tile_url,
                      const std::u16string& new_tile_title) const;

  void LoadInternalUrl(const GURL& url) const;

  const rebel::mojom::AutocompleteResultPtr& GetAutocompleteResult() const;
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete);
  void StopAutocomplete();
  void OpenAutocompleteMatch(uint32_t index,
                             const GURL& url,
                             bool middle_button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key);

  const rebel::mojom::RemoteNtpThemePtr& GetTheme() const;
  void ShowOrHideCustomizeMenu();

  const rebel::RemoteNtpWiFiStatusList& GetWiFiStatus() const;
  void UpdateWiFiStatus();

 private:
  RemoteNtp(const RemoteNtp&) = delete;
  RemoteNtp& operator=(const RemoteNtp&) = delete;

  // Overridden from content::RenderFrameObserver:
  void DidClearWindowObject() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void OnDestruct() override;

  // Overridden from rebel::mojom::RemoteNtpClient:
  void NtpTilesChanged(rebel::RemoteNtpTileList tiles) override;
  void AutocompleteResultChanged(
      rebel::mojom::AutocompleteResultPtr result) override;
  void ThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) override;
  void WiFiStatusChanged(rebel::RemoteNtpWiFiStatusList status) override;

  // The connection to the RemoteNTP service in the browser process.
  mojo::AssociatedRemote<rebel::mojom::RemoteNtp> remote_ntp_router_;
  mojo::AssociatedReceiver<rebel::mojom::RemoteNtpClient> receiver_;

  // Whether it's legal to execute JavaScript in |render_frame()|.
  // This class may want to execute JS in response to IPCs (via the
  // RemoteNtpExtension::Dispatch* methods). However, for cross-process
  // navigations, a "provisional frame" is created at first, and it's illegal
  // to execute any JS in it before it is actually swapped in, i.e. before the
  // navigation has committed. So this only gets set to true in
  // RenderFrameObserver::DidCommitProvisionalLoad. See crbug.com/765101.
  // Note: If crbug.com/794942 ever gets resolved, then it might be possible
  // to move the mojo connection code from the ctor to DidCommitProvisionalLoad
  // and avoid this bool.
  bool can_run_js_in_renderframe_;

  RemoteNtpTileCache ntp_tiles_cache_;
  bool has_received_ntp_tiles_;

  rebel::mojom::AutocompleteResultPtr autocomplete_result_;

  rebel::mojom::RemoteNtpThemePtr theme_;

  rebel::RemoteNtpWiFiStatusList wifi_status_;

  base::WeakPtrFactory<RemoteNtp> weak_ptr_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_H_
