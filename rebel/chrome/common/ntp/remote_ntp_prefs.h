// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_COMMON_NTP_REMOTE_NTP_PREFS_H_
#define REBEL_CHROME_COMMON_NTP_REMOTE_NTP_PREFS_H_

#include "base/feature_list.h"

class GURL;

namespace rebel {

extern const base::Feature kRemoteNtpFeature;

extern const char kRemoteNtpUrl[];
extern const char kRemoteNtpProcess[];

extern const char kRemoteNtpOfflineHost[];
extern const char kRemoteNtpOfflineUrl[];

extern const char kRemoteNtpLocalBackgroundPath[];
extern const char kRemoteNtpLocalBackgroundUrl[];

bool IsRemoteNtpEnabled();

const GURL& GetRemoteNtpUrl();
const GURL& GetRemoteNtpUrl(bool& from_command_line);

}  // namespace rebel

#endif  // REBEL_CHROME_COMMON_NTP_REMOTE_NTP_PREFS_H_
