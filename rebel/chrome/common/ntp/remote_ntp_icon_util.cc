// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/common/ntp/remote_ntp_icon_util.h"

#include <algorithm>

#include "base/check.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/url_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace rebel {

namespace {

constexpr const char kUrlParam[] = "url";
constexpr const char kSizeParam[] = "size";
constexpr const char kMonogramParam[] = "show_fallback_monogram";

int ParseIconSize(const std::string& value) {
  const std::vector<std::string> pieces = base::SplitString(
      value, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() != 2) {
    return 0;
  }

  int size_in_dip = 0;
  float device_scale_factor = 0.0;

  if (!base::StringToInt(pieces[0], &size_in_dip) ||
      !webui::ParseScaleFactor(pieces[1], &device_scale_factor)) {
    return 0;
  }

  return std::ceil(size_in_dip * device_scale_factor);
}

}  // namespace

const char kRemoteNtpIconPath[] = "icon";

rebel::mojom::RemoteNtpIconType ParseIconType(const std::string& icon_type) {
  if (base::EqualsCaseInsensitiveASCII(icon_type, "icon") ||
      base::EqualsCaseInsensitiveASCII(icon_type, "shortcut icon")) {
    return rebel::mojom::RemoteNtpIconType::Favicon;
  }

  if (base::EqualsCaseInsensitiveASCII(icon_type, "fluid-icon")) {
    return rebel::mojom::RemoteNtpIconType::Fluid;
  }

  if (base::EqualsCaseInsensitiveASCII(icon_type, "apple-touch-icon") ||
      base::EqualsCaseInsensitiveASCII(icon_type,
                                       "apple-touch-icon-precomposed")) {
    return rebel::mojom::RemoteNtpIconType::Touch;
  }

  return rebel::mojom::RemoteNtpIconType::Unknown;
}

rebel::mojom::RemoteNtpIconPtr GetPreferredIcon(
    const GURL& origin,
    std::vector<rebel::mojom::RemoteNtpIconPtr> icons) {
  auto comparator = [](const auto& a, const auto& b) {
    if (a->icon_type != b->icon_type) {
      return a->icon_type < b->icon_type;
    }

    return a->icon_size < b->icon_size;
  };

  auto it = std::max_element(icons.begin(), icons.end(), comparator);
  if (it == icons.end()) {
    auto icon = rebel::mojom::RemoteNtpIcon::New();
    icon->host_origin = origin;
    return icon;
  }

  rebel::mojom::RemoteNtpIconPtr largest_icon = std::move(*it);
  return largest_icon;
}

const RemoteNtpIcon ParseIconFromURL(const GURL& request) {
  RemoteNtpIcon parsed;
  if (!request.is_valid()) {
    return parsed;
  }

  for (net::QueryIterator it(request); !it.IsAtEnd(); it.Advance()) {
    base::StringPiece key = it.GetKey();
    const std::string& value = it.GetUnescapedValue();

    if (key == kUrlParam) {
      parsed.url = GURL(value);
    } else if (key == kSizeParam) {
      parsed.icon_size = ParseIconSize(value);
    } else if (key == kMonogramParam) {
      parsed.show_fallback_monogram = value != "false";
    }
  }

  return parsed;
}

}  // namespace rebel
