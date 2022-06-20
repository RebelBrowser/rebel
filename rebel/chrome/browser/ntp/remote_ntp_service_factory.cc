// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_factory.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

#include "rebel/chrome/browser/ntp/remote_ntp_service_impl.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

namespace rebel {

// static
RemoteNtpService* RemoteNtpServiceFactory::GetForProfile(Profile* profile) {
  if (!profile || profile->IsOffTheRecord()) {
    return nullptr;
  }

  return static_cast<RemoteNtpService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RemoteNtpServiceFactory* RemoteNtpServiceFactory::GetInstance() {
  return base::Singleton<RemoteNtpServiceFactory>::get();
}

RemoteNtpServiceFactory::RemoteNtpServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "RemoteNtpService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(TopSitesFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(chrome_colors::ChromeColorsFactory::GetInstance());
  DependsOn(NtpBackgroundServiceFactory::GetInstance());
  DependsOn(ThemeServiceFactory::GetInstance());
#endif
}

RemoteNtpServiceFactory::~RemoteNtpServiceFactory() = default;

content::BrowserContext* RemoteNtpServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* RemoteNtpServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!rebel::IsRemoteNtpEnabled()) {
    return nullptr;
  }

  return new RemoteNtpServiceImpl(Profile::FromBrowserContext(context));
}

}  // namespace rebel
