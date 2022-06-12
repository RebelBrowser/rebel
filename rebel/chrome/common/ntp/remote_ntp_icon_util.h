// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_COMMON_NTP_REMOTE_NTP_ICON_UTIL_H_
#define REBEL_CHROME_COMMON_NTP_REMOTE_NTP_ICON_UTIL_H_

#include <string>
#include <vector>

#include "build/build_config.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

namespace rebel {

extern const char kRemoteNtpIconPath[];

// Store results from |ParseIconFromURL| for a RemoteNTP tile request.
struct RemoteNtpIcon {
  GURL url;
  int icon_size{0};
  bool show_fallback_monogram{true};
};

// Interpret the provided HTML |rel| attribute as an icon type, returning
// rebel::mojom::RemoteNtpIconType::Unknown on failure.
rebel::mojom::RemoteNtpIconType ParseIconType(const std::string& icon_type);

// Select the best icon from the provided set of icons. We prefer the best icon
// type (i.e. touch icons over favicons), and use icon size as a tie breaker. If
// there wasn't an icon specified in the DOM, returns an empty icon containing
// only the given origin.
rebel::mojom::RemoteNtpIconPtr GetPreferredIcon(
    const GURL& origin,
    std::vector<rebel::mojom::RemoteNtpIconPtr> icons);

// Parses the path after chrome-search://remote-ntp-offline/icon. Example path:
// "?size=24@2x&url=https%3A%2F%2Fxkcd.com".
const RemoteNtpIcon ParseIconFromURL(const GURL& request);

}  // namespace rebel

#endif  // REBEL_CHROME_COMMON_NTP_REMOTE_NTP_ICON_UTIL_H_
