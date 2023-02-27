// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_CHANNEL_SELECTION_H_
#define REBEL_CHROME_BROWSER_CHANNEL_SELECTION_H_

#include <string>

class PrefRegistrySimple;
class PrefService;

namespace rebel {

extern const char kChannelPrefName[];
extern const char kChannelCommandLine[];

extern const char kChannelFlagName[];
extern const char kChannelFlagDescription[];

extern const char kChannelStable[];
extern const char kChannelAlpha[];
extern const char kChannelPreAlpha[];
extern const char kChannelCanary[];

// Register the profile preferences for channel selection.
void RegisterChannelSelectionProfilePrefs(PrefRegistrySimple* pref_registry);

// Store the current user's installed channel in the default user's preferences,
// if we haven't already.
void InitializeChannelSelection(PrefService* pref_service);

// Retrieve the update URL for the user's selected channel (or the installed
// channel, if none was selected).
std::string GetChannelUpdateURL(PrefService* pref_service);

// Update the channel installed in the default user's preferences.
void StoreSelectedChannel(PrefService* pref_service);

// Check if the user's selected channel differs from the installed channel.
bool DidUserSelectNewChannel(PrefService* pref_service);

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_CHANNEL_SELECTION_H_
