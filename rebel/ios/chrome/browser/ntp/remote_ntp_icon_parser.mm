// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ntp/remote_ntp_icon_parser.h"

#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"
#include "ios/web/public/js_messaging/script_message.h"
#import "ios/web/web_state/web_state_impl.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_icon_util.h"
#include "rebel/ios/chrome/browser/ntp/remote_ntp_service_factory_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr const char kScriptName[] = "favicon";
const char kScriptHandlerName[] = "TouchIconUrlsHandler";

std::vector<rebel::mojom::RemoteNtpIconPtr> ParseIconsFromMessage(
    const base::Value::List& favicons,
    const GURL& origin) {
  std::vector<rebel::mojom::RemoteNtpIconPtr> icons;

  for (const base::Value& favicon_value : favicons) {
    if (!favicon_value.is_dict()) {
      continue;
    }

    const auto& favicon = favicon_value.GetDict();
    const std::string* href = favicon.FindString("href");
    const std::string* rel = favicon.FindString("rel");
    const std::string* sizes = favicon.FindString("sizes");
    if (!href || !rel) {
      continue;
    }

    rebel::mojom::RemoteNtpIconType icon_type = rebel::ParseIconType(*rel);
    if (icon_type == rebel::mojom::RemoteNtpIconType::Unknown) {
      continue;
    }

    auto icon = rebel::mojom::RemoteNtpIcon::New();
    icon->host_origin = origin;
    icon->icon_url = GURL(*href);
    icon->icon_type = icon_type;

    if (sizes) {
      auto split_sizes = base::SplitStringPiece(*sizes, base::kWhitespaceASCII,
                                                base::TRIM_WHITESPACE,
                                                base::SPLIT_WANT_NONEMPTY);

      if (split_sizes.size() == 1) {
        auto dimensions = base::SplitStringPiece(
            split_sizes[0], "x", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

        int width = 0, height = 0;
        if ((dimensions.size() == 2) &&
            base::StringToInt(dimensions[0], &width) &&
            base::StringToInt(dimensions[1], &height)) {
          if ((width != 0) && (width == height)) {
            icon->icon_size = width;
          }
        }
      }
    }

    icons.push_back(std::move(icon));
  }

  return icons;
}

}  // namespace

namespace rebel {

RemoteNtpIconParser::RemoteNtpIconParser()
    : JavaScriptFeature(
          web::ContentWorld::kAllContentWorlds,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

RemoteNtpIconParser::~RemoteNtpIconParser() = default;

absl::optional<std::string> RemoteNtpIconParser::GetScriptMessageHandlerName()
    const {
  return kScriptHandlerName;
}

void RemoteNtpIconParser::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  DCHECK(message.is_main_frame());

  auto* remote_ntp_service = rebel::RemoteNtpServiceFactory::GetForBrowserState(
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState()));
  if (!remote_ntp_service || !remote_ntp_service->icon_storage()) {
    return;
  }

  if (!message.body() || !message.body()->is_list() || !message.request_url()) {
    return;
  }

  const auto& favicons = message.body()->GetList();
  const GURL origin = message.request_url().value().GetWithEmptyPath();
  if (!origin.is_valid() || !origin.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  auto icons = ParseIconsFromMessage(favicons, origin);
  auto icon = rebel::GetPreferredIcon(origin, std::move(icons));

  remote_ntp_service->icon_storage()->FetchIconIfNeeded(std::move(icon));
}

}  // namespace rebel
