// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_service_impl.h"

#include "base/bind.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#include "rebel/chrome/browser/ntp/remote_ntp_source.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

#if !defined(OS_ANDROID)
#include "rebel/chrome/browser/ntp/remote_ntp_theme_provider.h"
#endif

namespace rebel {

RemoteNtpServiceImpl::RemoteNtpServiceImpl(Profile* profile)
    : RemoteNtpService(profile->GetPath(), profile->GetPrefs()),
      profile_(profile),
      weak_factory_(this) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    return;
  }

  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

#if !defined(OS_ANDROID)
  // Themes are only available for desktop devices.
  remote_ntp_theme_provider_ =
      std::make_unique<rebel::RemoteNtpThemeProvider>(this, profile);
#endif

  InitializeService(ChromeMostVisitedSitesFactory::NewForProfile(profile_),
                    profile_->GetDefaultStoragePartition()
                        ->GetURLLoaderFactoryForBrowserProcess());

  // Set up the data sources that RemoteNTP uses.
  content::URLDataSource::Add(profile_,
                              std::make_unique<RemoteNtpSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));
#if !defined(OS_ANDROID)
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));
#endif
}

RemoteNtpServiceImpl::~RemoteNtpServiceImpl() = default;

// static
bool RemoteNtpServiceImpl::IsRemoteNtpUrl(const GURL& url) {
  // The effective URL for RemoteNTP may actually be e.g. chrome-native://newtab
  // or chrome-search://remote-ntp, depending on platform and page load state.
  if (url.SchemeIs(chrome::kChromeSearchScheme) ||
      url.SchemeIs(chrome::kChromeNativeScheme)) {
    return (url.host_piece() == chrome::kChromeUINewTabHost) ||
           (url.host_piece() == chrome::kChromeSearchRemoteNtpHost);
  }

  return false;
}

// static
bool RemoteNtpServiceImpl::ShouldAssignUrlToRemoteNtpRenderer(
    const GURL& url,
    Profile* profile) {
  if (!profile || profile->IsOffTheRecord() || !url.is_valid()) {
    return false;
  }

  return rebel::IsRemoteNtpEnabled() &&
         (url.SchemeIs(chrome::kChromeSearchScheme) ||
          RemoteNtpService::IsRemoteNtpUrl(url));
}

// static
bool RemoteNtpServiceImpl::ShouldUseProcessPerSiteForRemoteNtpUrl(
    const GURL& url,
    Profile* profile) {
  return ShouldAssignUrlToRemoteNtpRenderer(url, profile) &&
         ((url.host_piece() == rebel::kRemoteNtpOfflineHost) ||
          (url.host_piece() == chrome::kChromeSearchRemoteNtpHost));
}

// static
GURL RemoteNtpServiceImpl::GetEffectiveURLForRemoteNtp(const GURL& url) {
  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    return url;
  }

  // Replace the scheme with "chrome-search:", and clear the port, since
  // chrome-search is a scheme without port.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(chrome::kChromeSearchScheme);
  replacements.ClearPort();

  // If this is the URL for the server-provided NTP, replace the host with
  // "remote-ntp".
  replacements.SetHostStr(chrome::kChromeSearchRemoteNtpHost);

  return url.ReplaceComponents(replacements);
}

// static
bool RemoteNtpServiceImpl::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* remote_ntp_service = rebel::RemoteNtpServiceFactory::GetForProfile(
      static_cast<Profile*>(browser_context));

  if (!remote_ntp_service) {
    return false;
  }

  // The process_id for the navigation request will be -1. If so, allow this
  // request since it's not going to another renderer.
  return (process_id == -1) ||
         remote_ntp_service->IsRemoteNtpProcess(process_id);
}

void RemoteNtpServiceImpl::AddRemoteNtpProcess(int process_id) {
  process_ids_.insert(process_id);
}

bool RemoteNtpServiceImpl::IsRemoteNtpProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void RemoteNtpServiceImpl::RemoveRemoteNtpProcesses(int process_id) {
  process_ids_.erase(process_id);
}

std::unique_ptr<AutocompleteController>
RemoteNtpServiceImpl::CreateAutocompleteController() const {
  return std::make_unique<AutocompleteController>(
      std::make_unique<ChromeAutocompleteProviderClient>(profile_),
      AutocompleteClassifier::DefaultOmniboxProviders());
}

void RemoteNtpServiceImpl::Shutdown() {
  process_ids_.clear();

  RemoteNtpService::Shutdown();
}

void RemoteNtpServiceImpl::FetchBackgroundCollections() {
#if !defined(OS_ANDROID)
  remote_ntp_theme_provider_->FetchBackgroundCollections();
#endif
}

void RemoteNtpServiceImpl::FetchBackgroundImages(
    const std::string& collection_id) {
#if !defined(OS_ANDROID)
  remote_ntp_theme_provider_->FetchBackgroundImages(collection_id);
#endif
}

void RemoteNtpServiceImpl::StoreBackgroundImage(
    const std::string& collection_id,
    rebel::mojom::BackgroundImagePtr image) {
#if !defined(OS_ANDROID)
  remote_ntp_theme_provider_->StoreBackgroundImage(collection_id,
                                                   std::move(image));
#endif
}

rebel::mojom::RemoteNtpThemePtr RemoteNtpServiceImpl::CreateTheme() {
#if defined(OS_ANDROID)
  return RemoteNtpService::CreateTheme();
#else
  return remote_ntp_theme_provider_->CreateTheme();
#endif
}

void RemoteNtpServiceImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* render_process_host =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* renderer_profile =
          static_cast<Profile*>(render_process_host->GetBrowserContext());

      if (profile_ == renderer_profile) {
        RemoveRemoteNtpProcesses(render_process_host->GetID());
      }

      break;
    }

    default:
      NOTREACHED() << "Unexpected notification type in RemoteNtpServiceImpl.";
  }
}

}  // namespace rebel
