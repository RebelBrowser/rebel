// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_UI_WEBUI_REMOTE_NTP_INTERNALS_UI_H_
#define REBEL_CHROME_BROWSER_UI_WEBUI_REMOTE_NTP_INTERNALS_UI_H_

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include "ios/chrome/grit/ios_branded_strings.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"
#else
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_controller.h"
#endif

#include "rebel/grit/rebel_resources.h"

namespace rebel {

class RemoteNtpIconStorage;

extern const char kRemoteNtpInternalsHost[];

// The WebUI for chrome://remote-ntp-internals.
#if BUILDFLAG(IS_IOS)
class RemoteNtpInternalsUI : public web::WebUIIOSController {
#else
class RemoteNtpInternalsUI : public content::WebUIController {
#endif
 public:
#if BUILDFLAG(IS_IOS)
  explicit RemoteNtpInternalsUI(web::WebUIIOS* web_ui, const std::string& host);
#else
  explicit RemoteNtpInternalsUI(content::WebUI* web_ui);
#endif
  ~RemoteNtpInternalsUI() override = default;

 private:
  RemoteNtpInternalsUI(const RemoteNtpInternalsUI&) = delete;
  RemoteNtpInternalsUI& operator=(const RemoteNtpInternalsUI&) = delete;

  template <typename HtmlSourceType, typename WebUIType, typename HandlerType>
  void SetupHandlersAndResources(HtmlSourceType* html_source,
                                 WebUIType* web_ui,
                                 HandlerType handler) {
    web_ui->AddMessageHandler(std::move(handler));
    html_source->UseStringsJs();

    html_source->AddLocalizedString("remoteNtpInternalsTitle",
                                    IDS_REMOTE_NTP_INTERNALS_TITLE);
    html_source->AddLocalizedString("remoteNtpInternalsIcon",
                                    IDS_REMOTE_NTP_INTERNALS_ICON);
    html_source->AddLocalizedString("remoteNtpInternalsIconTableDelete",
                                    IDS_REMOTE_NTP_INTERNALS_ICON_DELETE);
    html_source->AddLocalizedString("remoteNtpInternalsIconTableOrigin",
                                    IDS_REMOTE_NTP_INTERNALS_ICON_ORIGIN);
    html_source->AddLocalizedString("remoteNtpInternalsIconTablePath",
                                    IDS_REMOTE_NTP_INTERNALS_ICON_PATH);
    html_source->AddLocalizedString("remoteNtpInternalsIconTableType",
                                    IDS_REMOTE_NTP_INTERNALS_ICON_TYPE);
    html_source->AddLocalizedString("remoteNtpInternalsIconTableImage",
                                    IDS_REMOTE_NTP_INTERNALS_ICON_IMAGE);

    html_source->SetDefaultResource(IDR_REMOTE_NTP_INTERNALS_HTML);
    html_source->AddResourcePath("remote_ntp_icon.css",
                                 IDR_REMOTE_NTP_INTERNALS_CSS);
    html_source->AddResourcePath("remote_ntp_icon.js",
                                 IDR_REMOTE_NTP_INTERNALS_JS);
  }
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_UI_WEBUI_REMOTE_NTP_INTERNALS_UI_H_
