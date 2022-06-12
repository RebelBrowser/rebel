// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

#include "base/command_line.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

namespace rebel {

const char kRemoteNtpCommandLine[] = "enable-remote-ntp";
const char kRemoteNtpUrl[] = "remote-ntp-url";

const char kRemoteNtpProcess[] = "remote-ntp-process";

const char kRemoteNtpOfflineHost[] = "remote-ntp-offline";
const char kRemoteNtpOfflineUrl[] =
    "chrome-search://remote-ntp-offline/index.html";

const char kRemoteNtpLocalBackgroundPath[] = "local_background.jpg";
const char kRemoteNtpLocalBackgroundUrl[] =
    "chrome-search://remote-ntp-offline/local_background.jpg";

const char kRemoteNtpDefaultVariant[] = "Rebel NTP";
const char kRemoteNtpDefaultUrl[] =
    "https://browser.viasat.com/rebel_ntp/index.html";

const char kRemoteNtpDevelopmentVariant[] = "Rebel NTP (Development version)";
const char kRemoteNtpDevelopmentUrl[] =
    "https://browser.viasat.com/rebel_ntp_dev/index.html";

const char kRemoteNtpVariantFlagName[] = "Rebel NTP Variant";
const char kRemoteNtpVariantFlagDescription[] =
    "Choose the variant of the remote New Tab Page to load.";

bool IsRemoteNtpEnabled() {
  // TODO(tflynn): Add a command line / pref to disable.
  return true;
}

std::string GetRemoteNtpUrl() {
  bool from_command_line = false;
  return GetRemoteNtpUrl(from_command_line);
}

std::string GetRemoteNtpUrl(bool& from_command_line) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  from_command_line = command_line->HasSwitch(rebel::kRemoteNtpUrl);

  if (from_command_line) {
    return command_line->GetSwitchValueASCII(rebel::kRemoteNtpUrl);
  } else if (IsRemoteNtpEnabled()) {
    return rebel::kRemoteNtpDefaultUrl;
  }

  // Empty string informs callers to use Chrome's default NTP.
  return {};
}

}  // namespace rebel
