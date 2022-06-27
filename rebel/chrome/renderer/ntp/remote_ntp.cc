// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/renderer/ntp/remote_ntp.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

#include "rebel/chrome/renderer/ntp/remote_ntp_extension.h"

namespace rebel {

namespace {

const constexpr size_t kMaxNtpTilesCacheSize = 100;

// Returns true if items stored in |current_tiles| and |new_tiles| are equal.
bool AreNtpTilesEqual(
    const RemoteNtp::RemoteNtpTileCache::ItemIDVector& current_tiles,
    const RemoteNtp::RemoteNtpTileCache::ItemVector& new_tiles) {
  if (current_tiles.size() != new_tiles.size()) {
    return false;
  }

  for (size_t i = 0; i < new_tiles.size(); ++i) {
    if ((new_tiles[i].title != current_tiles[i].second.title) ||
        (new_tiles[i].url != current_tiles[i].second.url)) {
      return false;
    }
  }

  return true;
}

}  // namespace

RemoteNtp::RemoteNtp(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<RemoteNtp>(render_frame),
      receiver_(this),
      can_run_js_in_renderframe_(false),
      ntp_tiles_cache_(kMaxNtpTilesCacheSize),
      has_received_ntp_tiles_(false),
      weak_ptr_factory_(this) {
  // Connect to the RemoteNTP interface in the browser.
  mojo::AssociatedRemote<rebel::mojom::RemoteNtpConnector> connector;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&connector);

  mojo::PendingAssociatedRemote<rebel::mojom::RemoteNtpClient>
      remote_ntp_client;
  receiver_.Bind(remote_ntp_client.InitWithNewEndpointAndPassReceiver(),
                 render_frame->GetTaskRunner(
                     blink::TaskType::kInternalNavigationAssociated));

  connector->Connect(remote_ntp_router_.BindNewEndpointAndPassReceiver(
                         render_frame->GetTaskRunner(
                             blink::TaskType::kInternalNavigationAssociated)),
                     std::move(remote_ntp_client));
}

RemoteNtp::~RemoteNtp() = default;

void RemoteNtp::DidClearWindowObject() {
  rebel::RemoteNtpExtension::Install(render_frame()->GetWebFrame());
}

void RemoteNtp::DidCommitProvisionalLoad(ui::PageTransition transition) {
  can_run_js_in_renderframe_ = true;
}

void RemoteNtp::OnDestruct() {
  delete this;
}

bool RemoteNtp::AreNtpTilesAvailable() const {
  return has_received_ntp_tiles_;
}

void RemoteNtp::GetNtpTiles(RemoteNtpTileCache::ItemIDVector* tiles) const {
  ntp_tiles_cache_.GetCurrentItems(tiles);
}

void RemoteNtp::AddCustomTile(const GURL& tile_url,
                              const std::u16string& tile_title) const {
  remote_ntp_router_->AddCustomTile(tile_url, tile_title);
}

void RemoteNtp::RemoveCustomTile(const GURL& tile_url) const {
  remote_ntp_router_->RemoveCustomTile(tile_url);
}

void RemoteNtp::EditCustomTile(const GURL& old_tile_url,
                               const GURL& new_tile_url,
                               const std::u16string& new_tile_title) const {
  remote_ntp_router_->EditCustomTile(old_tile_url, new_tile_url,
                                     new_tile_title);
}

void RemoteNtp::LoadInternalUrl(const GURL& url) const {
  remote_ntp_router_->LoadInternalUrl(url);
}

const rebel::mojom::AutocompleteResultPtr& RemoteNtp::GetAutocompleteResult()
    const {
  return autocomplete_result_;
}

void RemoteNtp::QueryAutocomplete(const std::u16string& input,
                                  bool prevent_inline_autocomplete) {
  remote_ntp_router_->QueryAutocomplete(input, prevent_inline_autocomplete);
}

void RemoteNtp::StopAutocomplete() {
  remote_ntp_router_->StopAutocomplete();
}

void RemoteNtp::OpenAutocompleteMatch(uint32_t index,
                                      const GURL& url,
                                      bool middle_button,
                                      bool alt_key,
                                      bool ctrl_key,
                                      bool meta_key,
                                      bool shift_key) {
  remote_ntp_router_->OpenAutocompleteMatch(index, url, middle_button, alt_key,
                                            ctrl_key, meta_key, shift_key);
}

const rebel::RemoteNtpBackgroundCollectionList&
RemoteNtp::GetBackgroundCollections() const {
  return background_collections_;
}

const rebel::RemoteNtpBackgroundImageMap& RemoteNtp::GetBackgroundImages()
    const {
  return background_images_;
}

const rebel::mojom::RemoteNtpThemePtr& RemoteNtp::GetTheme() const {
  return theme_;
}

void RemoteNtp::LoadBackgroundCollections() {
  remote_ntp_router_->LoadBackgroundCollections();
}

void RemoteNtp::LoadBackgroundImages(const std::string& collection_id) {
  remote_ntp_router_->LoadBackgroundImages(collection_id);
}

void RemoteNtp::SetBackgroundImage(const std::string& collection_id,
                                   rebel::mojom::BackgroundImagePtr image) {
  remote_ntp_router_->SetBackgroundImage(collection_id, std::move(image));
}

void RemoteNtp::SelectLocalBackgroundImage() {
  remote_ntp_router_->SelectLocalBackgroundImage();
}

void RemoteNtp::PreviewColor(SkColor color) {
  remote_ntp_router_->PreviewColor(color);
}

void RemoteNtp::RevertColor() {
  remote_ntp_router_->RevertColor();
}

void RemoteNtp::CommitColor() {
  remote_ntp_router_->CommitColor();
}

const rebel::RemoteNtpWiFiStatusList& RemoteNtp::GetWiFiStatus() const {
  return wifi_status_;
}

void RemoteNtp::UpdateWiFiStatus() {
  remote_ntp_router_->UpdateWiFiStatus();
}

void RemoteNtp::NtpTilesChanged(rebel::RemoteNtpTileList tiles) {
  has_received_ntp_tiles_ = true;

  RemoteNtpTileCache::ItemIDVector current_tiles;
  GetNtpTiles(&current_tiles);

  RemoteNtpTileCache::ItemVector new_tiles;
  for (auto& tile : tiles) {
    new_tiles.push_back(tile->To<rebel::mojom::RemoteNtpTile>());
  }

  if (AreNtpTilesEqual(current_tiles, new_tiles)) {
    return;
  }

  ntp_tiles_cache_.AddItems(new_tiles);

  if (can_run_js_in_renderframe_) {
    RemoteNtpExtension::DispatchNtpTilesChanged(render_frame()->GetWebFrame());
  }
}

void RemoteNtp::AutocompleteResultChanged(
    rebel::mojom::AutocompleteResultPtr result) {
  autocomplete_result_ = std::move(result);

  if (can_run_js_in_renderframe_) {
    RemoteNtpExtension::DispatchAutocompleteResultChanged(
        render_frame()->GetWebFrame());
  }
}

void RemoteNtp::BackgroundCollectionsChanged(
    rebel::RemoteNtpBackgroundCollectionList collections) {
  background_collections_ = std::move(collections);

  if (can_run_js_in_renderframe_) {
    RemoteNtpExtension::DispatchBackgroundCollectionsChanged(
        render_frame()->GetWebFrame());
  }
}

void RemoteNtp::BackgroundImagesChanged(
    rebel::RemoteNtpBackgroundImageMap images) {
  background_images_ = std::move(images);

  if (can_run_js_in_renderframe_) {
    RemoteNtpExtension::DispatchBackgroundImagesChanged(
        render_frame()->GetWebFrame());
  }
}

void RemoteNtp::LocalBackgroundImageSelected() {
  if (can_run_js_in_renderframe_) {
    RemoteNtpExtension::DispatchLocalBackgroundImageSelected(
        render_frame()->GetWebFrame());
  }
}

void RemoteNtp::ThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) {
  theme_ = std::move(theme);

  if (can_run_js_in_renderframe_) {
    RemoteNtpExtension::DispatchThemeChanged(render_frame()->GetWebFrame());
  }
}

void RemoteNtp::WiFiStatusChanged(rebel::RemoteNtpWiFiStatusList wifi_status) {
  wifi_status_ = std::move(wifi_status);

  if (can_run_js_in_renderframe_) {
    RemoteNtpExtension::DispatchWiFiStatusChanged(
        render_frame()->GetWebFrame());
  }
}

}  // namespace rebel
