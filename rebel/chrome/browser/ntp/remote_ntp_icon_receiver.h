// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_RECEIVER_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_RECEIVER_H_

#include "base/memory/raw_ptr.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace rebel {

// Browser-side receiver for large icons parsed by the renderer.
class RemoteNtpIconReceiver : public rebel::mojom::RemoteNtpIconReceiver {
 public:
  RemoteNtpIconReceiver(Profile* profile);

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<rebel::mojom::RemoteNtpIconReceiver> receiver);

  void IconParsed(rebel::mojom::RemoteNtpIconPtr icon) override;

 private:
  RemoteNtpIconReceiver(const RemoteNtpIconReceiver&) = delete;
  RemoteNtpIconReceiver& operator=(const RemoteNtpIconReceiver&) = delete;

  raw_ptr<Profile> profile_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_RECEIVER_H_
