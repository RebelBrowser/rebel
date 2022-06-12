// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

#include "base/command_line.h"
#include "base/no_destructor.h"

#include "rebel/build/buildflag.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

namespace rebel {

BASE_FEATURE(kRemoteNtpFeature, "RemoteNTP", base::FEATURE_ENABLED_BY_DEFAULT);

const char kRemoteNtpUrl[] = "remote-ntp-url";
const char kRemoteNtpProcess[] = "remote-ntp-process";

const char kRemoteNtpOfflineHost[] = "remote-ntp-offline";
const char kRemoteNtpOfflineUrl[] =
    "chrome-search://remote-ntp-offline/index.html";

const char kRemoteNtpLocalBackgroundPath[] = "local_background.jpg";
const char kRemoteNtpLocalBackgroundUrl[] =
    "chrome-search://remote-ntp-offline/local_background.jpg";

bool IsRemoteNtpEnabled() {
  return base::FeatureList::IsEnabled(kRemoteNtpFeature);
}

const GURL& GetRemoteNtpUrl() {
  bool from_command_line = false;
  return GetRemoteNtpUrl(from_command_line);
}

const GURL& GetRemoteNtpUrl(bool& from_command_line) {
  static bool url_was_from_command_line = false;

  static base::NoDestructor<GURL> remote_ntp_url([]() -> GURL {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    url_was_from_command_line = command_line->HasSwitch(rebel::kRemoteNtpUrl);

    if (url_was_from_command_line) {
      return GURL(command_line->GetSwitchValueASCII(rebel::kRemoteNtpUrl));
    } else if (IsRemoteNtpEnabled()) {
      return GURL(REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NEW_TAB_PAGE));
    }

    return {};
  }());

  from_command_line = url_was_from_command_line;
  return *remote_ntp_url;
}

}  // namespace rebel
