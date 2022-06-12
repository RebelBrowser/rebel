// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"

#include <string>

#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

#if !BUILDFLAG(IS_IOS)
#include "rebel/chrome/browser/ntp/remote_ntp_service_impl.h"
#endif

namespace rebel {

namespace {

const constexpr size_t kMostVisitedSitesSize = 10;

// Returns true if |url_a| matches |url_b| in terms of their origin and path.
bool MatchesOriginAndPath(const GURL& url_a, const GURL& url_b) {
  return (url_a.scheme_piece() == url_b.scheme_piece()) &&
         (url_a.host_piece() == url_b.host_piece()) &&
         (url_a.port() == url_b.port()) &&
         (url_a.path_piece() == url_b.path_piece());
}

}  // namespace

RemoteNtpService::RemoteNtpService(const base::FilePath& profile_path,
                                   PrefService* pref_service)
    : profile_path_(profile_path),
      pref_service_(pref_service),
      weak_factory_(this) {}

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
         (url.host_piece() == rebel::kRemoteNtpOfflineHost);
}

bool RemoteNtpService::IsRemoteNtpProcess(int process_id) const {
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

  NotifyAboutBackgroundCollections();
  NotifyAboutBackgroundImages();
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

void RemoteNtpService::LoadBackgroundCollections() {
  if (background_collections_.empty()) {
    FetchBackgroundCollections();
  } else {
    NotifyAboutBackgroundCollections();
  }
}

void RemoteNtpService::LoadBackgroundImages(const std::string& collection_id) {
  if (background_images_.find(collection_id) == background_images_.end()) {
    FetchBackgroundImages(collection_id);
  } else {
    NotifyAboutBackgroundImages();
  }
}

void RemoteNtpService::SetBackgroundImage(
    const std::string& collection_id,
    rebel::mojom::BackgroundImagePtr image) {
  StoreBackgroundImage(collection_id, std::move(image));
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

void RemoteNtpService::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  ntp_tiles_.clear();

  for (const auto& section_and_tiles : sections) {
    for (const auto& tile : section_and_tiles.second) {
      ntp_tiles_.push_back(rebel::mojom::RemoteNtpTile::New(
          tile.title, tile.url.spec(), tile.favicon_url.spec()));
    }
  }

  NotifyAboutNtpTiles();
}

void RemoteNtpService::OnIconMadeAvailable(const GURL& site_url) {}

void RemoteNtpService::OnBackgroundCollectionsAvailable(
    rebel::RemoteNtpBackgroundCollectionList collections) {
  background_collections_ = std::move(collections);
  NotifyAboutBackgroundCollections();
}

void RemoteNtpService::OnBackgroundImagesAvailable(
    const std::string& collection,
    rebel::RemoteNtpBackgroundImageList images) {
  background_images_[collection] = std::move(images);
  NotifyAboutBackgroundImages();
}

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

void RemoteNtpService::NotifyAboutBackgroundCollections() {
  for (Observer& observer : observers_) {
    observer.OnBackgroundCollectionsChanged(background_collections_);
  }
}

void RemoteNtpService::NotifyAboutBackgroundImages() {
  for (Observer& observer : observers_) {
    observer.OnBackgroundImagesChanged(background_images_);
  }
}

void RemoteNtpService::NotifyAboutTheme() {
  for (Observer& observer : observers_) {
    observer.OnThemeChanged(theme_->Clone());
  }
}

}  // namespace rebel
