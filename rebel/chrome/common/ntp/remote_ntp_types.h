// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_COMMON_NTP_REMOTE_NTP_TYPES_H_
#define REBEL_CHROME_COMMON_NTP_REMOTE_NTP_TYPES_H_

#include <vector>

#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"

// Mojo doesn't support type aliases, so long type names used by RemoteNTP are
// defined here for convenience.
namespace rebel {

using RemoteNtpTileList = std::vector<rebel::mojom::RemoteNtpTilePtr>;
using RemoteNtpWiFiStatusList = std::vector<rebel::mojom::WiFiStatusPtr>;

}  // namespace rebel

#endif  // REBEL_CHROME_COMMON_NTP_REMOTE_NTP_TYPES_H_
