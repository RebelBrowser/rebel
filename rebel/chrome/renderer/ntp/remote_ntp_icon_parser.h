// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_ICON_PARSER_H_
#define REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_ICON_PARSER_H_

#include "content/public/renderer/render_frame_observer.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace rebel {

// RenderFrameObserver to parse a page's <head> tag for any of the following
// children:
//
//     <link rel="icon" ...>
//     <link rel="shortcut icon" ...>
//     <link rel="fluid-icon" ...>
//     <link rel="apple-touch-icon" ...>
//     <link rel="apple-touch-icon-precomposed" ...>
//
// The largest icon is sent to the browser process for fetching and storage.
class RemoteNtpIconParser : public content::RenderFrameObserver {
 public:
  RemoteNtpIconParser(content::RenderFrame* render_frame);

 private:
  void OnDestruct() override;
  void DidFinishLoad() override;
};

}  // namespace rebel

#endif  // REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_ICON_PARSER_H_
