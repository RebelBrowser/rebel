// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/resource/resource_bundle.h"

#if !BUILDFLAG(IS_IOS)
#include "base/no_destructor.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service_impl.h"
#endif

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"
#include "rebel/grit/rebel_resources.h"
#include "rebel/services/network/remote_ntp_api_allow_list.h"

namespace rebel {

namespace {

constexpr const char kServiceWorkerFileName[] = "service-worker.js";

constexpr const size_t kMostVisitedSitesSize = 8;
constexpr const char kDefaultSitesUrl[] =
    BUILDFLAG(REBEL_BROWSER_DEFAULT_SITES);

// Creates the list of popular sites based on a snapshot.
base::Value DefaultSites() {
  absl::optional<base::Value> sites = base::JSONReader::Read(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_REMOTE_NTP_DEFAULT_SITES_JSON));

  return std::move(sites.value());
}

// Returns true if |url_a| matches |url_b| in terms of their origin.
bool MatchesOrigin(const GURL& url_a, const GURL& url_b) {
  return (url_a.scheme_piece() == url_b.scheme_piece()) &&
         (url_a.host_piece() == url_b.host_piece()) &&
         (url_a.port() == url_b.port());
}

// Returns true if |url_a| matches |url_b| in terms of their origin and path.
bool MatchesOriginAndPath(const GURL& url_a, const GURL& url_b) {
  return MatchesOrigin(url_a, url_b) &&
         (url_a.path_piece() == url_b.path_piece());
}

// Returns true if |url| matches the service worker URL for |document_url|.
bool MatchesServiceWorker(const GURL& url, const GURL& document_url) {
  if (!MatchesOrigin(url, document_url)) {
    return false;
  }

  return url.ExtractFileName() == kServiceWorkerFileName;
}

}  // namespace

RemoteNtpService::RemoteNtpService(const base::FilePath& profile_path,
                                   PrefService* pref_service)
    : profile_path_(profile_path),
      pref_service_(pref_service),
      weak_factory_(this) {
  if (pref_service_) {
    pref_service_->SetString(ntp_tiles::prefs::kPopularSitesOverrideURL,
                             kDefaultSitesUrl);
    pref_service_->Set(ntp_tiles::prefs::kPopularSitesJsonPref, DefaultSites());
  }
}

RemoteNtpService::~RemoteNtpService() = default;

// static
bool RemoteNtpService::IsRemoteNtpUrl(const GURL& url) {
  const GURL& remote_ntp_url = rebel::GetRemoteNtpUrl();
  if (!remote_ntp_url.is_valid() || !url.is_valid()) {
    return false;
  }

#if !BUILDFLAG(IS_IOS)
  if (RemoteNtpServiceImpl::IsRemoteNtpUrl(url)) {
    return true;
  }
#endif

  return MatchesOriginAndPath(url, remote_ntp_url) ||
         MatchesServiceWorker(url, remote_ntp_url) ||
         (url.host_piece() == rebel::kRemoteNtpOfflineHost);
}

bool RemoteNtpService::IsRemoteNtpProcess(int process_id) const {
  return false;
}

bool RemoteNtpService::IsRemoteNtpAPIProcess(int process_id) const {
  return false;
}

void RemoteNtpService::InitializeService(
    std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (most_visited) {
    most_visited_ = std::move(most_visited);

    most_visited_->AddMostVisitedURLsObserver(this, kMostVisitedSitesSize);
    most_visited_->EnableCustomLinks(true);
  }

  if (url_loader_factory) {
    icon_storage_ = std::make_unique<rebel::RemoteNtpIconStorage>(
        this, profile_path_, pref_service_, url_loader_factory);

    remote_ntp_api_allow_list_ = std::make_unique<rebel::RemoteNtpApiAllowList>(
        pref_service_, url_loader_factory);
    remote_ntp_api_allow_list_->MaybeStartFetch(true);
  }
}

void RemoteNtpService::Shutdown() {
  if (most_visited_) {
    most_visited_->RemoveMostVisitedURLsObserver(this);
    most_visited_.reset();
  }
}

rebel::mojom::RemoteNtpThemePtr RemoteNtpService::CreateTheme() {
  return rebel::mojom::RemoteNtpTheme::New();
}

void RemoteNtpService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RemoteNtpService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RemoteNtpService::OnNewTabPageOpened() {
  theme_ = CreateTheme();

  if (most_visited_) {
    most_visited_->Refresh();
    NotifyAboutNtpTiles();
  }

  NotifyAboutTheme();
}

void RemoteNtpService::AddCustomTile(const GURL& tile_url,
                                     const std::u16string& tile_title) {
  if (most_visited_) {
    most_visited_->AddCustomLink(tile_url, tile_title);
  }
}

void RemoteNtpService::RemoveCustomTile(const GURL& tile_url) {
  if (most_visited_) {
    most_visited_->DeleteCustomLink(tile_url);
  }
}

void RemoteNtpService::EditCustomTile(const GURL& old_tile_url,
                                      const GURL& new_tile_url,
                                      const std::u16string& new_tile_title) {
  if (most_visited_) {
    most_visited_->UpdateCustomLink(old_tile_url, new_tile_url, new_tile_title);
  }
}

void RemoteNtpService::SetDarkModeEnabled(bool dark_mode_enabled) {
  if (!theme_) {
    theme_ = CreateTheme();
  }

  if (theme_->dark_mode_enabled != dark_mode_enabled) {
    theme_->dark_mode_enabled = dark_mode_enabled;
    NotifyAboutTheme();
  }
}

void RemoteNtpService::OnWiFiStatusChanged(
    rebel::RemoteNtpWiFiStatusList wifi_status) {
  wifi_status_ = std::move(wifi_status);
  NotifyAboutWiFiStatus();
}

void RemoteNtpService::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
#if !BUILDFLAG(IS_IOS)
  static base::NoDestructor<GURL> web_store_url(
      l10n_util::GetStringUTF8(IDS_WEBSTORE_URL));
#endif

  ntp_tiles_.clear();

  for (const auto& section_and_tiles : sections) {
    for (const auto& tile : section_and_tiles.second) {
#if !BUILDFLAG(IS_IOS)
      // Skip the built-in tile for the Chrome web store.
      if (tile.url == *web_store_url) {
        continue;
      }
#endif

      ntp_tiles_.push_back(rebel::mojom::RemoteNtpTile::New(
          tile.title, tile.url.spec(), tile.favicon_url.spec()));
    }
  }

  NotifyAboutNtpTiles();
}

void RemoteNtpService::OnIconMadeAvailable(const GURL& site_url) {}

void RemoteNtpService::OnThemeUpdated() {
  theme_ = CreateTheme();
  NotifyAboutTheme();
}

void RemoteNtpService::OnIconStored(const rebel::mojom::RemoteNtpIconPtr& icon,
                                    const base::FilePath& icon_file) {
  for (Observer& observer : observers_) {
    observer.OnTouchIconStored(icon, icon_file);
  }
}

void RemoteNtpService::OnIconEvicted(const GURL& origin,
                                     const base::FilePath& icon_file) {
  for (Observer& observer : observers_) {
    observer.OnTouchIconEvicted(origin, icon_file);
  }
}

void RemoteNtpService::OnIconLoadComplete(const GURL& origin, bool successful) {
  for (Observer& observer : observers_) {
    observer.OnTouchIconLoadComplete(origin, successful);
  }
}

void RemoteNtpService::NotifyAboutNtpTiles() {
  for (Observer& observer : observers_) {
    observer.OnNtpTilesChanged(ntp_tiles_);
  }
}

void RemoteNtpService::NotifyAboutTheme() {
  for (Observer& observer : observers_) {
    observer.OnThemeChanged(theme_->Clone());
  }
}

void RemoteNtpService::NotifyAboutWiFiStatus() {
  for (Observer& observer : observers_) {
    observer.OnWiFiStatusChanged(wifi_status_);
  }
}

}  // namespace rebel
