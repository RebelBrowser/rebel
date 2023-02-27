// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/channel_selection.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#include "rebel/build/buildflag.h"

namespace rebel {

const char kChannelPrefName[] =
    REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME_PATH) "_channel";
const char kChannelCommandLine[] =
    REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME_PATH) "-channel";

const char kChannelFlagName[] =
    REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME) " Channel";
const char kChannelFlagDescription[] =
    "Choose the channel to use when checking for browser updates. After "
    "choosing a channel, restart your browser and navigate to "
    REBEL_STRING_BUILDFLAG(REBEL_BROWSER_SCHEMA) "://help to install the "
    "latest version on that channel. WARNING: If you choose a channel on a "
    "lower major version, you should first back up your bookmarks and delete "
    "your existing profile.";

const char kChannelStable[] = "stable";
const char kChannelAlpha[] = "alpha";
const char kChannelPreAlpha[] = "prealpha";
const char kChannelCanary[] = "canary";

namespace {

constexpr const char kStableOmahaHost[] = BUILDFLAG(REBEL_OMAHA_PUBLIC_URL);
constexpr const char kInternalOmahaHost[] = BUILDFLAG(REBEL_OMAHA_PRIVATE_URL);

constexpr const char kOmahaUrlFormat[] = "%s/sparkle/" REBEL_STRING_BUILDFLAG(
    REBEL_SPARKLE_PRODUCT_NAME) "/%s/appcast.xml";

std::string GetSelectedChannel(PrefService* pref_service) {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kChannelCommandLine)) {
    std::string override_channel =
        command_line->GetSwitchValueASCII(kChannelCommandLine);

    if ((override_channel == kChannelStable) ||
        (override_channel == kChannelAlpha) ||
        (override_channel == kChannelPreAlpha) ||
        (override_channel == kChannelCanary)) {
      return override_channel;
    }
  }

  if (pref_service) {
    return pref_service->GetString(kChannelPrefName);
  }

  return kChannelStable;
}

}  // namespace

void RegisterChannelSelectionProfilePrefs(PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterStringPref(kChannelPrefName, std::string());
}

void InitializeChannelSelection(PrefService* pref_service) {
  std::string channel = pref_service->GetString(kChannelPrefName);

  if (channel.empty()) {
    channel = kChannelStable;
    pref_service->SetString(kChannelPrefName, channel);
  }
}

std::string GetChannelUpdateURL(PrefService* pref_service) {
  auto channel = GetSelectedChannel(pref_service);

  if (channel == kChannelStable) {
    return base::StringPrintf(kOmahaUrlFormat, kStableOmahaHost,
                              channel.c_str());
  }

  return base::StringPrintf(kOmahaUrlFormat, kInternalOmahaHost,
                            channel.c_str());
}

void StoreSelectedChannel(PrefService* pref_service) {
  if (!pref_service) {
    return;
  }

  auto channel = GetSelectedChannel(pref_service);
  pref_service->SetString(kChannelPrefName, channel);
}

bool DidUserSelectNewChannel(PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }

  auto channel = GetSelectedChannel(pref_service);
  return channel != pref_service->GetString(kChannelPrefName);
}

}  // namespace rebel
