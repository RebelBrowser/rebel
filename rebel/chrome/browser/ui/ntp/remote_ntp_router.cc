// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ui/ntp/remote_ntp_router.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"

namespace rebel {

namespace {

bool IsInRemoteNtpProcess(content::RenderFrameHost* render_frame) {
  content::RenderProcessHost* process_host = render_frame->GetProcess();

  const RemoteNtpService* remote_ntp_service =
      RemoteNtpServiceFactory::GetForProfile(
          Profile::FromBrowserContext(process_host->GetBrowserContext()));
  if (!remote_ntp_service) {
    return false;
  }

  int process_id = process_host->GetID();
  return remote_ntp_service->IsRemoteNtpProcess(process_id) ||
         remote_ntp_service->IsRemoteNtpAPIProcess(process_id);
}

class RemoteNtpClientFactoryImpl
    : public RemoteNtpRouter::RemoteNtpClientFactory,
      public rebel::mojom::RemoteNtpConnector {
 public:
  // |web_contents| and |receiver| must outlive this object.
  RemoteNtpClientFactoryImpl(
      content::WebContents* web_contents,
      mojo::AssociatedReceiver<rebel::mojom::RemoteNtp>* receiver)
      : client_receiver_(receiver), factory_receivers_(web_contents, this) {
    DCHECK(web_contents);
    DCHECK(receiver);

    // Before we are connected to a frame we throw away all messages.
    remote_ntp_client_.reset();
  }

  rebel::mojom::RemoteNtpClient* GetRemoteNtpClient() override {
    return remote_ntp_client_.is_bound() ? remote_ntp_client_.get() : nullptr;
  }

  void BindFactoryReceiver(
      mojo::PendingAssociatedReceiver<rebel::mojom::RemoteNtpConnector>
          receiver,
      content::RenderFrameHost* render_frame_host) override {
    factory_receivers_.Bind(render_frame_host, std::move(receiver));
  }

 private:
  RemoteNtpClientFactoryImpl(const RemoteNtpClientFactoryImpl&) = delete;
  RemoteNtpClientFactoryImpl& operator=(const RemoteNtpClientFactoryImpl&) =
      delete;

  void Connect(
      mojo::PendingAssociatedReceiver<rebel::mojom::RemoteNtp> receiver,
      mojo::PendingAssociatedRemote<rebel::mojom::RemoteNtpClient> client)
      override {
    content::RenderFrameHost* render_frame_host =
        factory_receivers_.GetCurrentTargetFrame();

    const bool is_main_frame = render_frame_host->GetParent() == nullptr;
    const bool is_remote_ntp_process = IsInRemoteNtpProcess(render_frame_host);

    if (is_main_frame && is_remote_ntp_process) {
      client_receiver_->reset();
      client_receiver_->Bind(std::move(receiver));

      remote_ntp_client_.reset();
      remote_ntp_client_.Bind(std::move(client));
    }
  }

  // An interface used to push updates to the frame that connected to us. Before
  // we've been connected to a frame, messages sent on this interface go into
  // the void.
  mojo::AssociatedRemote<rebel::mojom::RemoteNtpClient> remote_ntp_client_;

  // Used to bind incoming pending receivers to the implementation, which lives
  // in RemoteNtpRouter.
  mojo::AssociatedReceiver<rebel::mojom::RemoteNtp>* client_receiver_;

  // Receivers used to listen to connection requests.
  content::RenderFrameHostReceiverSet<rebel::mojom::RemoteNtpConnector>
      factory_receivers_;
};

}  // namespace

RemoteNtpRouter::RemoteNtpRouter(content::WebContents* web_contents,
                                 Delegate* delegate)
    : WebContentsObserver(web_contents),
      delegate_(delegate),
      receiver_(this),
      remote_ntp_client_factory_(
          new RemoteNtpClientFactoryImpl(web_contents, &receiver_)) {
  DCHECK(web_contents);
  DCHECK(delegate);
}

RemoteNtpRouter::~RemoteNtpRouter() = default;

void RemoteNtpRouter::BindRemoteNtpConnector(
    mojo::PendingAssociatedReceiver<rebel::mojom::RemoteNtpConnector> receiver,
    content::RenderFrameHost* render_frame_host) {
  remote_ntp_client_factory_->BindFactoryReceiver(std::move(receiver),
                                                  render_frame_host);
}

void RemoteNtpRouter::SendNtpTilesChanged(
    const rebel::RemoteNtpTileList& ntp_tiles) {
  if (!remote_ntp_client()) {
    return;
  }

  std::vector<rebel::mojom::RemoteNtpTilePtr> tiles;
  for (const auto& tile : ntp_tiles) {
    tiles.push_back(tile->Clone());
  }

  remote_ntp_client()->NtpTilesChanged(std::move(tiles));
}

void RemoteNtpRouter::SendAutocompleteResultChanged(
    rebel::mojom::AutocompleteResultPtr result) {
  if (!remote_ntp_client()) {
    return;
  }

  remote_ntp_client()->AutocompleteResultChanged(std::move(result));
}

void RemoteNtpRouter::SendBackgroundCollectionsChanged(
    const rebel::RemoteNtpBackgroundCollectionList& collections) {
  if (!remote_ntp_client()) {
    return;
  }

  rebel::RemoteNtpBackgroundCollectionList mojo_collections;
  for (const auto& collection : collections) {
    mojo_collections.push_back(collection->Clone());
  }

  remote_ntp_client()->BackgroundCollectionsChanged(
      std::move(mojo_collections));
}

void RemoteNtpRouter::SendBackgroundImagesChanged(
    const rebel::RemoteNtpBackgroundImageMap& images) {
  if (!remote_ntp_client()) {
    return;
  }

  rebel::RemoteNtpBackgroundImageMap mojom_images;
  for (const auto& id_and_images : images) {
    std::vector<rebel::mojom::BackgroundImagePtr> background_images;

    for (const auto& image : id_and_images.second) {
      background_images.push_back(image->Clone());
    }

    mojom_images.emplace(id_and_images.first, std::move(background_images));
  }

  remote_ntp_client()->BackgroundImagesChanged(std::move(mojom_images));
}

void RemoteNtpRouter::SendLocalBackgroundImageSelected() {
  if (!remote_ntp_client()) {
    return;
  }

  remote_ntp_client()->LocalBackgroundImageSelected();
}

void RemoteNtpRouter::SendThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) {
  if (!remote_ntp_client()) {
    return;
  }

  remote_ntp_client()->ThemeChanged(std::move(theme));
}

void RemoteNtpRouter::AddCustomTile(const GURL& tile_url,
                                    const std::u16string& tile_title) {
  delegate_->OnAddCustomTile(tile_url, tile_title);
}

void RemoteNtpRouter::RemoveCustomTile(const GURL& tile_url) {
  delegate_->OnRemoveCustomTile(tile_url);
}

void RemoteNtpRouter::EditCustomTile(const GURL& old_tile_url,
                                     const GURL& new_tile_url,
                                     const std::u16string& new_tile_title) {
  delegate_->OnEditCustomTile(old_tile_url, new_tile_url, new_tile_title);
}

void RemoteNtpRouter::LoadInternalUrl(const GURL& url) {
  delegate_->OnLoadInternalUrl(url);
}

void RemoteNtpRouter::QueryAutocomplete(const std::u16string& input,
                                        bool prevent_inline_autocomplete) {
  delegate_->OnQueryAutocomplete(input, prevent_inline_autocomplete);
}

void RemoteNtpRouter::StopAutocomplete() {
  delegate_->OnStopAutocomplete();
}

void RemoteNtpRouter::OpenAutocompleteMatch(uint32_t index,
                                            const GURL& url,
                                            bool middle_button,
                                            bool alt_key,
                                            bool ctrl_key,
                                            bool meta_key,
                                            bool shift_key) {
  delegate_->OnOpenAutocompleteMatch(index, url, middle_button, alt_key,
                                     ctrl_key, meta_key, shift_key);
}

void RemoteNtpRouter::LoadBackgroundCollections() {
  delegate_->OnLoadBackgroundCollections();
}

void RemoteNtpRouter::LoadBackgroundImages(const std::string& collection_id) {
  delegate_->OnLoadBackgroundImages(collection_id);
}

void RemoteNtpRouter::SetBackgroundImage(
    const std::string& collection_id,
    rebel::mojom::BackgroundImagePtr image) {
  delegate_->OnSetBackgroundImage(collection_id, std::move(image));
}

void RemoteNtpRouter::SelectLocalBackgroundImage() {
  delegate_->OnSelectLocalBackgroundImage();
}

void RemoteNtpRouter::PreviewColor(SkColor color) {
  delegate_->OnPreviewColor(color);
}

void RemoteNtpRouter::RevertColor() {
  delegate_->OnRevertColor();
}

void RemoteNtpRouter::CommitColor() {
  delegate_->OnCommitColor();
}

}  // namespace rebel
