// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_FACTORY_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace rebel {

class RemoteNtpService;

// Singleton that owns all RemoteNtpServices and associates them with Profiles.
class RemoteNtpServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the RemoteNtpService for |profile|.
  static RemoteNtpService* GetForProfile(Profile* profile);

  static RemoteNtpServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<RemoteNtpServiceFactory>;

  RemoteNtpServiceFactory();
  ~RemoteNtpServiceFactory() override;

  RemoteNtpServiceFactory(const RemoteNtpServiceFactory&) = delete;
  RemoteNtpServiceFactory& operator=(const RemoteNtpServiceFactory&) = delete;

  // Overridden from BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_FACTORY_H_
