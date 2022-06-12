// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_EXTENSION_H_
#define REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_EXTENSION_H_

namespace blink {
class WebLocalFrame;
}

namespace rebel {

// Javascript bindings for the window.rebel APIs.
class RemoteNtpExtension {
 public:
  static void Install(blink::WebLocalFrame* frame);

  // Helpers to dispatch Javascript events.
  static void DispatchNtpTilesChanged(blink::WebLocalFrame* frame);
  static void DispatchAutocompleteResultChanged(blink::WebLocalFrame* frame);
  static void DispatchThemeChanged(blink::WebLocalFrame* frame);
  static void DispatchWiFiStatusChanged(blink::WebLocalFrame* frame);

 private:
  RemoteNtpExtension() = delete;
  RemoteNtpExtension(const RemoteNtpExtension&) = delete;
  RemoteNtpExtension& operator=(const RemoteNtpExtension&) = delete;
};

}  // namespace rebel

#endif  // REBEL_CHROME_RENDERER_NTP_REMOTE_NTP_EXTENSION_H_
