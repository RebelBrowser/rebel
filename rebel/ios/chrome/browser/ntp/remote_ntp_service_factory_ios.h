// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_FACTORY_IOS_H_
#define REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_FACTORY_IOS_H_

#include <memory>

#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace rebel {

class RemoteNtpService;

// Singleton that owns all RemoteNtpServices and associates them with
// ChromeBrowserStates.
class RemoteNtpServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the RemoteNtpService for |browser_state|.
  static RemoteNtpService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static RemoteNtpServiceFactory* GetInstance();

 private:
  RemoteNtpServiceFactory(const RemoteNtpServiceFactory&) = delete;
  RemoteNtpServiceFactory& operator=(const RemoteNtpServiceFactory&) = delete;

  friend struct base::DefaultSingletonTraits<RemoteNtpServiceFactory>;

  RemoteNtpServiceFactory();
  ~RemoteNtpServiceFactory() override;

  // Overridden from BrowserStateKeyedServiceFactory:
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace rebel

#endif  // REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_FACTORY_IOS_H_
