// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/rebel_main_extra_parts.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "rebel/chrome/browser/channel_selection.h"
#endif

namespace rebel {

void RebelMainExtraParts::PostProfileInit(Profile* profile,
                                          bool is_initial_profile) {
  if (!profile) {
    return;
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  auto* pref_service = profile->GetPrefs();

  if (pref_service) {
    InitializeChannelSelection(pref_service);
  }
#endif
}

}  // namespace rebel
