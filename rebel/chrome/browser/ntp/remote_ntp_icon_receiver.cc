// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_icon_receiver.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"

namespace rebel {

RemoteNtpIconReceiver::RemoteNtpIconReceiver(Profile* profile)
    : profile_(profile) {}

// static
void RemoteNtpIconReceiver::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<rebel::mojom::RemoteNtpIconReceiver> receiver) {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());

  mojo::MakeSelfOwnedReceiver(std::make_unique<RemoteNtpIconReceiver>(profile),
                              std::move(receiver));
}

void RemoteNtpIconReceiver::IconParsed(rebel::mojom::RemoteNtpIconPtr icon) {
  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(profile_);

  if (remote_ntp_service && remote_ntp_service->icon_storage()) {
    remote_ntp_service->icon_storage()->FetchIconIfNeeded(std::move(icon));
  }
}

}  // namespace rebel
