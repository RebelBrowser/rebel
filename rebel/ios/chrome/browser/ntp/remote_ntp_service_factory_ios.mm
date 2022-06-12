// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "rebel/ios/chrome/browser/ntp/remote_ntp_service_factory_ios.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/history/model/top_sites_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"
#import "rebel/ios/chrome/browser/ntp/remote_ntp_service_ios.h"

namespace rebel {

// static
RemoteNtpService* RemoteNtpServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  if (browser_state->IsOffTheRecord()) {
    return nullptr;
  }

  return static_cast<RemoteNtpService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
RemoteNtpServiceFactory* RemoteNtpServiceFactory::GetInstance() {
  return base::Singleton<RemoteNtpServiceFactory>::get();
}

RemoteNtpServiceFactory::RemoteNtpServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "RemoteNtpService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::TopSitesFactory::GetInstance());
  DependsOn(IOSChromeLargeIconCacheFactory::GetInstance());
  DependsOn(IOSChromeLargeIconServiceFactory::GetInstance());
}

RemoteNtpServiceFactory::~RemoteNtpServiceFactory() = default;

web::BrowserState* RemoteNtpServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

std::unique_ptr<KeyedService> RemoteNtpServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!rebel::IsRemoteNtpEnabled()) {
    return nullptr;
  }

  return std::make_unique<RemoteNtpServiceIos>(
      ChromeBrowserState::FromBrowserState(context));
}

}  // namespace rebel
