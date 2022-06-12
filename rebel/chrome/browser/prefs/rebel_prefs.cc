// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/prefs/rebel_prefs.h"

#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#include "components/ntp_tiles/custom_links_manager_impl.h"
#else
#include "components/ntp_tiles/popular_sites_impl.h"
#endif

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/services/network/remote_ntp_api_allow_list.h"

namespace rebel {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  rebel::RemoteNtpApiAllowList::RegisterProfilePrefs(registry);
  rebel::RemoteNtpIconStorage::RegisterProfilePrefs(registry);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  ntp_tiles::CustomLinksManagerImpl::RegisterProfilePrefs(registry);
#else
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
#endif
}

}  // namespace rebel
