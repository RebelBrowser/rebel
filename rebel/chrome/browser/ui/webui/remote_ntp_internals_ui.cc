// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ui/webui/remote_ntp_internals_ui.h"

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_IOS)
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

#include "rebel/ios/chrome/browser/ntp/remote_ntp_service_factory_ios.h"
#else
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#endif

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"

namespace rebel {

namespace {

// Same size the NTP currently uses, so we can see what it actually looks like.
constexpr const int kIconSize = 85;
constexpr const char kHostOriginPref[] = "host_origin";

static std::string GetPNGDataUrl(const SkBitmap& bitmap) {
  std::vector<unsigned char> data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data);

  std::string url;
  url.insert(url.end(), data.begin(), data.end());

  url = base::Base64Encode(url);
  url.insert(0, "data:image/png;base64,");

  return url;
}

#if BUILDFLAG(IS_IOS)
class RemoteNtpHandler : public web::WebUIIOSMessageHandler {
#else
class RemoteNtpHandler : public content::WebUIMessageHandler {
#endif
 public:
  RemoteNtpHandler(rebel::RemoteNtpIconStorage* remote_ntp_icon_storage)
      : remote_ntp_icon_storage_(remote_ntp_icon_storage),
        weak_factory_(this) {}
  ~RemoteNtpHandler() override = default;

  // Overridden from WebUIMessageHandler
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "requestCachedIcons",
        base::BindRepeating(&RemoteNtpHandler::HandleRequestCachedIcons,
                            weak_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "deleteCachedIcon",
        base::BindRepeating(&RemoteNtpHandler::HandleDeleteCachedIcon,
                            weak_factory_.GetWeakPtr()));
  }

 private:
  RemoteNtpHandler(const RemoteNtpHandler&) = delete;
  RemoteNtpHandler& operator=(const RemoteNtpHandler&) = delete;

  void HandleRequestCachedIcons(const base::Value::List&) {
    if (!remote_ntp_icon_storage_) {
      return;
    }

    SendCachedIcons();
  }

  void HandleDeleteCachedIcon(const base::Value::List& args) {
    if (!remote_ntp_icon_storage_) {
      return;
    }

    DCHECK_EQ(args.size(), 1U) << "Expected |origin| string argument";

    const base::Value& arg = args[0];
    DCHECK(arg.is_string()) << "Expected |origin| string argument";

    const GURL origin(arg.GetString());
    DCHECK(origin.is_valid());

    if (remote_ntp_icon_storage_->DeleteIconForOrigin(origin)) {
      SendCachedIcons();
    }
  }

  void SendCachedIcons() {
    base::Value icons(base::Value::Type::LIST);
    remote_ntp_icon_storage_->SerializeCachedIcons(icons.GetList());

    for (const base::Value& icon : icons.GetList()) {
      const std::string* host_origin =
          icon.GetDict().FindString(kHostOriginPref);
      DCHECK(host_origin);

      const GURL origin(*host_origin);

      remote_ntp_icon_storage_->GetIconForOrigin(
          origin, kIconSize,
          base::BindOnce(&RemoteNtpHandler::SendIconImage,
                         weak_factory_.GetWeakPtr(), origin));
    }

#if BUILDFLAG(IS_IOS)
    base::ValueView args[] = {icons};
    web_ui()->CallJavascriptFunction("cachedIconsAvailable", args);
#else
    AllowJavascript();
    CallJavascriptFunction("cachedIconsAvailable", icons);
#endif
  }

  void SendIconImage(const GURL& origin, SkBitmap bitmap) {
    base::Value icon(base::Value::Type::DICT);
    icon.GetDict().Set("origin", origin.spec());
    icon.GetDict().Set("icon", GetPNGDataUrl(bitmap));

#if BUILDFLAG(IS_IOS)
    base::ValueView args[] = {icon};
    web_ui()->CallJavascriptFunction("iconImageAvailable", args);
#else
    AllowJavascript();
    CallJavascriptFunction("iconImageAvailable", icon);
#endif
  }

  raw_ptr<rebel::RemoteNtpIconStorage> remote_ntp_icon_storage_;

  base::WeakPtrFactory<RemoteNtpHandler> weak_factory_;
};

}  // namespace

const char kRemoteNtpInternalsHost[] = "remote-ntp-internals";

#if BUILDFLAG(IS_IOS)

RemoteNtpInternalsUI::RemoteNtpInternalsUI(web::WebUIIOS* web_ui,
                                           const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);

  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForBrowserState(browser_state);
  rebel::RemoteNtpIconStorage* remote_ntp_icon_storage = nullptr;
  if (remote_ntp_service) {
    remote_ntp_icon_storage = remote_ntp_service->icon_storage();
  }

  auto* html_source =
      web::WebUIIOSDataSource::Create(rebel::kRemoteNtpInternalsHost);

  SetupHandlersAndResources(
      html_source, web_ui,
      std::make_unique<RemoteNtpHandler>(remote_ntp_icon_storage));

  web::WebUIIOSDataSource::Add(browser_state, html_source);
}

#else

RemoteNtpInternalsUI::RemoteNtpInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(profile);
  rebel::RemoteNtpIconStorage* remote_ntp_icon_storage = nullptr;
  if (remote_ntp_service) {
    remote_ntp_icon_storage = remote_ntp_service->icon_storage();
  }

  auto* html_source = content::WebUIDataSource::CreateAndAdd(
      profile, rebel::kRemoteNtpInternalsHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");

  SetupHandlersAndResources(
      html_source, web_ui,
      std::make_unique<RemoteNtpHandler>(remote_ntp_icon_storage));
}

#endif

}  // namespace rebel
