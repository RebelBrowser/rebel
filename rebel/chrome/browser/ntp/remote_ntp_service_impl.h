// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IMPL_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/render_process_host_observer.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"

class AutocompleteController;
class AutocompleteControllerDelegate;
class Profile;

class RemoteNtpThemeTest;
class RemoteNtpWiFiTest;

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

namespace wifi {
class WiFiService;
}  // namespace wifi

namespace rebel {

class RemoteNtpThemeProvider;
class RemoteNtpWifiService;

// Implementation of RemoteNtpService for desktop and Android devices. Tracks
// render process host IDs that are associated with RemoteNTP.
class RemoteNtpServiceImpl : public RemoteNtpService,
                             public content::RenderProcessHostObserver {
 public:
  explicit RemoteNtpServiceImpl(Profile* profile);
  ~RemoteNtpServiceImpl() override;

  static bool IsRemoteNtpUrl(const GURL& url);

  static bool ShouldAssignUrlToRemoteNtpRenderer(const GURL& url,
                                                 Profile* profile);
  static bool ShouldUseProcessPerSiteForRemoteNtpUrl(const GURL& url,
                                                     Profile* profile);
  static bool ShouldAllowUrlToUseRemoteNtpAPI(const GURL& url,
                                              Profile* profile);
  static GURL GetEffectiveURLForRemoteNtp(const GURL& url);
  static GURL GetEffectiveURLForRemoteNtpAPI(const GURL& url);

  // Determine if this chrome-search: request is coming from a RemoteNTP
  // renderer process.
  static bool ShouldServiceRequest(const GURL& url,
                                   content::BrowserContext* browser_context,
                                   int process_id);

  // Overridden from RemoteNtpService:
  void AddRemoteNtpProcess(int process_id) override;
  bool IsRemoteNtpProcess(int process_id) const override;
  void AddRemoteNtpAPIProcess(int process_id) override;
  bool IsRemoteNtpAPIProcess(int process_id) const override;
  void RemoveRemoteNtpProcesses(int process_id);
  std::unique_ptr<AutocompleteController> CreateAutocompleteController()
      const override;

 private:
  RemoteNtpServiceImpl(const RemoteNtpServiceImpl&) = delete;
  RemoteNtpServiceImpl& operator=(const RemoteNtpServiceImpl&) = delete;

  friend class ::RemoteNtpThemeTest;
  friend class ::RemoteNtpWiFiTest;

  // Overridden from RemoteNtpService:
  void Shutdown() final;
  rebel::mojom::RemoteNtpThemePtr CreateTheme() override;
  void UpdateWiFiStatus() override;

  // Overridden from content::RenderProcessHostObserver:
  void RenderProcessHostDestroyed(
      content::RenderProcessHost* render_process_host) override;

#if !BUILDFLAG(IS_ANDROID)
  RemoteNtpThemeProvider* GetThemeProviderForTesting() const {
    return remote_ntp_theme_provider_.get();
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<RemoteNtpThemeProvider> remote_ntp_theme_provider_;
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  void SetWiFiService(std::unique_ptr<wifi::WiFiService> wifi_service);
  std::unique_ptr<RemoteNtpWifiService> remote_ntp_wifi_service_;
#endif

  // The process ids associated with RemoteNTP processes.
  std::set<int> process_ids_;
  std::set<int> api_process_ids_;

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<RemoteNtpServiceImpl> weak_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_IMPL_H_
