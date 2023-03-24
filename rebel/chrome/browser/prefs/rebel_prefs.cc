// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/prefs/rebel_prefs.h"

#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#include "components/ntp_tiles/custom_links_manager_impl.h"
#else
#include "components/ntp_tiles/popular_sites_impl.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "rebel/chrome/browser/channel_selection.h"
#endif

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/services/network/remote_ntp_api_allow_list.h"

namespace rebel {

void RegisterLocalState(PrefRegistrySimple* registry) {
#if BUILDFLAG(REBEL_CRASH_REPORT_ENABLED)
  registry->SetDefaultPrefValue(metrics::prefs::kMetricsReportingEnabled,
                                base::Value(true));
#endif
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  rebel::RemoteNtpApiAllowList::RegisterProfilePrefs(registry);
  rebel::RemoteNtpIconStorage::RegisterProfilePrefs(registry);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  rebel::RegisterChannelSelectionProfilePrefs(registry);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  ntp_tiles::CustomLinksManagerImpl::RegisterProfilePrefs(registry);
#else
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
#endif
}

}  // namespace rebel
