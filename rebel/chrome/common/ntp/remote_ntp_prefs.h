// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_COMMON_NTP_REMOTE_NTP_PREFS_H_
#define REBEL_CHROME_COMMON_NTP_REMOTE_NTP_PREFS_H_

#include <string>

#include "base/feature_list.h"

namespace rebel {

extern const char kRemoteNtpCommandLine[];
extern const char kRemoteNtpUrl[];

extern const char kRemoteNtpProcess[];

extern const char kRemoteNtpOfflineHost[];
extern const char kRemoteNtpOfflineUrl[];

extern const char kRemoteNtpLocalBackgroundPath[];
extern const char kRemoteNtpLocalBackgroundUrl[];

extern const char kRemoteNtpDefaultVariant[];
extern const char kRemoteNtpDefaultUrl[];

extern const char kRemoteNtpDevelopmentVariant[];
extern const char kRemoteNtpDevelopmentUrl[];

extern const char kRemoteNtpVariantFlagName[];
extern const char kRemoteNtpVariantFlagDescription[];

bool IsRemoteNtpEnabled();

std::string GetRemoteNtpUrl();
std::string GetRemoteNtpUrl(bool& from_command_line);

}  // namespace rebel

#endif  // REBEL_CHROME_COMMON_NTP_REMOTE_NTP_PREFS_H_
