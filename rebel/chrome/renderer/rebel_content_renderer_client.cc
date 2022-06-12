// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/renderer/rebel_content_renderer_client.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "url/gurl.h"

#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"
#include "rebel/chrome/renderer/ntp/remote_ntp.h"
#include "rebel/chrome/renderer/ntp/remote_ntp_icon_parser.h"

namespace rebel {

RebelContentRendererClient::RebelContentRendererClient()
    : ChromeContentRendererClient() {}

RebelContentRendererClient::~RebelContentRendererClient() = default;

void RebelContentRendererClient::RenderThreadStarted() {
  ChromeContentRendererClient::RenderThreadStarted();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(rebel::kRemoteNtpProcess)) {
    return;
  }

  static base::NoDestructor<blink::WebURL> url(rebel::GetRemoteNtpUrl());
  if (!url->IsValid()) {
    return;
  }

  auto chrome_search_scheme =
      blink::WebString::FromASCII(chrome::kChromeSearchScheme);
  auto chrome_favicon_host =
      blink::WebString::FromASCII(chrome::kChromeUIFavicon2Host);
  auto remote_ntp_offline_host =
      blink::WebString::FromASCII(rebel::kRemoteNtpOfflineHost);
  int destination_port = 0;

  blink::WebSecurityPolicy::AddOriginAccessAllowListEntry(
      *url, chrome_search_scheme, chrome_favicon_host, destination_port,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  blink::WebSecurityPolicy::AddOriginAccessAllowListEntry(
      *url, chrome_search_scheme, remote_ntp_offline_host, destination_port,
      network::mojom::CorsDomainMatchMode::kAllowSubdomains,
      network::mojom::CorsPortMatchMode::kAllowAnyPort,
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
}

void RebelContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  ChromeContentRendererClient::RenderFrameCreated(render_frame);

  if (!render_frame->IsMainFrame() || !rebel::IsRemoteNtpEnabled()) {
    return;
  }

  new rebel::RemoteNtpIconParser(render_frame);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(rebel::kRemoteNtpProcess)) {
    new rebel::RemoteNtp(render_frame);
  }
}

}  // namespace rebel
