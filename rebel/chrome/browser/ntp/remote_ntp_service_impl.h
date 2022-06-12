// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IMPL_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

class AutocompleteController;
class AutocompleteControllerDelegate;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace rebel {

class RemoteNtpThemeProvider;

// Implementation of RemoteNtpService for desktop and Android devices. Tracks
// render process host IDs that are associated with RemoteNTP.
class RemoteNtpServiceImpl : public RemoteNtpService,
                             public content::NotificationObserver {
 public:
  explicit RemoteNtpServiceImpl(Profile* profile);
  ~RemoteNtpServiceImpl() override;

  static bool IsRemoteNtpUrl(const GURL& url);

  static bool ShouldAssignUrlToRemoteNtpRenderer(const GURL& url,
                                                 Profile* profile);
  static bool ShouldUseProcessPerSiteForRemoteNtpUrl(const GURL& url,
                                                     Profile* profile);
  static GURL GetEffectiveURLForRemoteNtp(const GURL& url);

  // Determine if this chrome-search: request is coming from a RemoteNTP
  // renderer process.
  static bool ShouldServiceRequest(const GURL& url,
                                   content::BrowserContext* browser_context,
                                   int process_id);

  // Overridden from RemoteNtpService:
  void AddRemoteNtpProcess(int process_id) override;
  bool IsRemoteNtpProcess(int process_id) const override;
  void RemoveRemoteNtpProcesses(int process_id);
  std::unique_ptr<AutocompleteController> CreateAutocompleteController()
      const override;

#if !defined(OS_ANDROID)
  // Used only for testing.
  RemoteNtpThemeProvider* GetThemeProviderForTesting() const {
    return remote_ntp_theme_provider_.get();
  }
#endif

 private:
  RemoteNtpServiceImpl(const RemoteNtpServiceImpl&) = delete;
  RemoteNtpServiceImpl& operator=(const RemoteNtpServiceImpl&) = delete;

  // Overridden from RemoteNtpService:
  void Shutdown() final;
  void FetchBackgroundCollections() override;
  void FetchBackgroundImages(const std::string& collection_id) override;
  void StoreBackgroundImage(const std::string& collection_id,
                            rebel::mojom::BackgroundImagePtr image) override;
  rebel::mojom::RemoteNtpThemePtr CreateTheme() override;

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

#if !defined(OS_ANDROID)
  std::unique_ptr<RemoteNtpThemeProvider> remote_ntp_theme_provider_;
#endif

  // The process ids associated with RemoteNTP processes.
  std::set<int> process_ids_;

  Profile* const profile_;
  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<RemoteNtpServiceImpl> weak_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IMPL_H_
