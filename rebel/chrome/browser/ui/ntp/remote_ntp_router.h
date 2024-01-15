// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_ROUTER_H_
#define REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_ROUTER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

class GURL;

namespace rebel {

// RemoteNtpRouter is responsible for receiving and sending IPC messages between
// the browser and the RemoteNTP.
class RemoteNtpRouter : public content::WebContentsObserver,
                        public rebel::mojom::RemoteNtp {
 public:
  // RemoteNtpRouter calls its delegate in response to messages received from
  // the RemoteNTP.
  class Delegate {
   public:
    // Called when the user wants to add a custom tile.
    virtual void OnAddCustomTile(const GURL& tile_url,
                                 const std::u16string& tile_title) = 0;

    // Called when the user wants to remove a custom tile.
    virtual void OnRemoveCustomTile(const GURL& tile_url) = 0;

    // Called when the user wants to update a custom tile.
    virtual void OnEditCustomTile(const GURL& old_tile_url,
                                  const GURL& new_tile_url,
                                  const std::u16string& new_tile_title) = 0;

    // Called when the RemoteNTP wants to open an internal (chrome://) URL.
    virtual void OnLoadInternalUrl(const GURL& url) = 0;

    // Called when the RemoteNTP wants to obtain autocomplete results.
    virtual void OnQueryAutocomplete(const std::u16string& input,
                                     bool prevent_inline_autocomplete) = 0;

    // Called when the RemoteNTP wants to cancel an autocomplete query.
    virtual void OnStopAutocomplete() = 0;

    // Called when the RemoteNTP wants to navigate to an autocomplete match.
    virtual void OnOpenAutocompleteMatch(uint32_t index,
                                         const GURL& url,
                                         bool middle_button,
                                         bool alt_key,
                                         bool ctrl_key,
                                         bool meta_key,
                                         bool shift_key) = 0;

    // Called when the RemoteNTP wants to show the Customize Chrome panel.
    virtual void OnShowOrHideCustomizeMenu() = 0;

    // Called when the RemoteNTP wants to retrieve the device's WiFi status.
    virtual void OnUpdateWiFiStatus() = 0;
  };

  // Creates rebel::mojom::RemoteNtpClient connections on request.
  class RemoteNtpClientFactory {
   public:
    RemoteNtpClientFactory() = default;
    virtual ~RemoteNtpClientFactory() = default;

    virtual rebel::mojom::RemoteNtpClient* GetRemoteNtpClient() = 0;

    virtual void BindFactoryReceiver(
        mojo::PendingAssociatedReceiver<rebel::mojom::RemoteNtpConnector>
            receiver,
        content::RenderFrameHost* render_frame_host) = 0;

   private:
    RemoteNtpClientFactory(const RemoteNtpClientFactory&) = delete;
    RemoteNtpClientFactory& operator=(const RemoteNtpClientFactory&) = delete;
  };

  RemoteNtpRouter(content::WebContents* web_contents, Delegate* delegate);
  ~RemoteNtpRouter() override;

  void BindRemoteNtpConnector(mojo::PendingAssociatedReceiver<
                                  rebel::mojom::RemoteNtpConnector> receiver,
                              content::RenderFrameHost* render_frame_host);

  void SendNtpTilesChanged(const rebel::RemoteNtpTileList& ntp_tiles);
  void SendAutocompleteResultChanged(
      rebel::mojom::AutocompleteResultPtr result);
  void SendThemeChanged(rebel::mojom::RemoteNtpThemePtr theme);
  void SendWiFiStatusChanged(const rebel::RemoteNtpWiFiStatusList& status);

 private:
  RemoteNtpRouter(const RemoteNtpRouter&) = delete;
  RemoteNtpRouter& operator=(const RemoteNtpRouter&) = delete;

  // Overridden from rebel::mojom::RemoteNtp:
  void AddCustomTile(const GURL& tile_url,
                     const std::u16string& tile_title) override;
  void RemoveCustomTile(const GURL& tile_url) override;
  void EditCustomTile(const GURL& old_tile_url,
                      const GURL& new_tile_url,
                      const std::u16string& new_tile_title) override;
  void LoadInternalUrl(const GURL& url) override;
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete) override;
  void StopAutocomplete() override;
  void OpenAutocompleteMatch(uint32_t index,
                             const GURL& url,
                             bool middle_button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key) override;
  void ShowOrHideCustomizeMenu() override;
  void UpdateWiFiStatus() override;

  rebel::mojom::RemoteNtpClient* remote_ntp_client() const {
    return remote_ntp_client_factory_->GetRemoteNtpClient();
  }

  raw_ptr<Delegate> delegate_;

  // Receiver for the connected main frame. We only allow one frame to connect
  // at the moment, but this could be extended to a map of connected frames, if
  // desired.
  mojo::AssociatedReceiver<rebel::mojom::RemoteNtp> receiver_;

  std::unique_ptr<RemoteNtpClientFactory> remote_ntp_client_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_ROUTER_H_
