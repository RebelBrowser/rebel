// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/renderer/ntp/remote_ntp_icon_parser.h"

#include <string>
#include <vector>

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "url/gurl.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_icon_util.h"

namespace rebel {

namespace {

void ParseIconFromElement(const GURL& origin,
                          const blink::WebElement& link,
                          std::vector<rebel::mojom::RemoteNtpIconPtr>& icons) {
  const std::string rel = link.GetAttribute("rel").Utf8();

  const rebel::mojom::RemoteNtpIconType icon_type = rebel::ParseIconType(rel);
  if (icon_type == rebel::mojom::RemoteNtpIconType::Unknown) {
    return;
  }

  const blink::WebString href = link.GetAttribute("href");
  if (href.IsNull() || href.IsEmpty()) {
    return;
  }

  GURL icon_url = link.GetDocument().CompleteURL(href);
  if (!icon_url.is_valid()) {
    return;
  }

  auto icon = rebel::mojom::RemoteNtpIcon::New();
  icon->host_origin = origin;
  icon->icon_url = std::move(icon_url);
  icon->icon_type = icon_type;

  if (link.HasAttribute("sizes")) {
    const blink::WebVector<gfx::Size> icon_sizes =
        blink::WebIconSizesParser::ParseIconSizes(link.GetAttribute("sizes"));

    if ((icon_sizes.size() == 1) && (icon_sizes[0].width() != 0) &&
        (icon_sizes[0].height() == icon_sizes[0].width())) {
      icon->icon_size = icon_sizes[0].width();
    }
  }

  icons.push_back(std::move(icon));
}

std::vector<rebel::mojom::RemoteNtpIconPtr> ParseIconsFromFrame(
    blink::WebDocument& document,  // Mutable because |Head| is non-const.
    const GURL& origin) {
  std::vector<rebel::mojom::RemoteNtpIconPtr> icons;

  const blink::WebElement head = document.Head();
  if (head.IsNull()) {
    return icons;
  }

  for (blink::WebNode child = head.FirstChild(); !child.IsNull();
       child = child.NextSibling()) {
    if (!child.IsElementNode()) {
      continue;
    }

    const blink::WebElement element = child.To<blink::WebElement>();
    if (!element.HasHTMLTagName("link")) {
      continue;
    }

    ParseIconFromElement(origin, element, icons);
  }

  return icons;
}

}  // namespace

RemoteNtpIconParser::RemoteNtpIconParser(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

void RemoteNtpIconParser::OnDestruct() {
  delete this;
}

void RemoteNtpIconParser::DidFinishLoad() {
  blink::WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  if (document.IsNull()) {
    return;
  }

  const GURL document_url = document.Url();
  const GURL origin = document_url.GetWithEmptyPath();
  if (!origin.is_valid() || !origin.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  auto icons = ParseIconsFromFrame(document, origin);
  auto icon = rebel::GetPreferredIcon(origin, std::move(icons));

  mojo::Remote<rebel::mojom::RemoteNtpIconReceiver> receiver;
  render_frame()->GetBrowserInterfaceBroker()->GetInterface(
      receiver.BindNewPipeAndPassReceiver());

  receiver->IconParsed(std::move(icon));
}

}  // namespace rebel
