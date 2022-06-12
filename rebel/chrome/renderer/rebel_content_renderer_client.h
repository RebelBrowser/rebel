// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_RENDERER_REBEL_CONTENT_RENDERER_CLIENT_H_
#define REBEL_CHROME_RENDERER_REBEL_CONTENT_RENDERER_CLIENT_H_

#include "chrome/renderer/chrome_content_renderer_client.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace rebel {

class RebelContentRendererClient : public ChromeContentRendererClient {
 public:
  RebelContentRendererClient();
  ~RebelContentRendererClient() override;

  // content::ContentRendererClient:
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;

 private:
  RebelContentRendererClient(const RebelContentRendererClient&) = delete;
  RebelContentRendererClient& operator=(const RebelContentRendererClient&) =
      delete;
};

}  // namespace rebel

#endif  // REBEL_CHROME_RENDERER_REBEL_CONTENT_RENDERER_CLIENT_H_
