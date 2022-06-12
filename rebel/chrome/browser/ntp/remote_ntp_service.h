// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/chrome/browser/ntp/remote_ntp_theme_delegate.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

class AutocompleteController;
class AutocompleteControllerDelegate;
class GURL;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace rebel {

// Per-profile service responsible for tracking information (e.g. most visited
// tiles) required by the RemoteNTP.
class RemoteNtpService : public KeyedService,
                         public ntp_tiles::MostVisitedSites::Observer,
                         public rebel::RemoteNtpThemeDelegate,
                         public rebel::RemoteNtpIconStorage::Delegate {
 public:
  class Observer {
   public:
    // Indicates that the stored NTP tiles have changed in some way.
    virtual void OnNtpTilesChanged(const rebel::RemoteNtpTileList& ntp_tiles) {}

    // Indicates that the background collections have changed in some way.
    virtual void OnBackgroundCollectionsChanged(
        const rebel::RemoteNtpBackgroundCollectionList& collections) {}

    // Indicates that the images for a background collection have changed in
    // some way.
    virtual void OnBackgroundImagesChanged(
        const rebel::RemoteNtpBackgroundImageMap& images) {}

    // Indicates that the browser or system theme has changed in some way.
    virtual void OnThemeChanged(rebel::mojom::RemoteNtpThemePtr theme) {}

    // Indicates that a touch icon has been stored or updated.
    virtual void OnTouchIconStored(const rebel::mojom::RemoteNtpIconPtr& icon,
                                   const base::FilePath& icon_file) {}

    // Indicates that a touch icon has evicted from storage.
    virtual void OnTouchIconEvicted(const GURL& origin,
                                    const base::FilePath& icon_file) {}

    // Indicates that a touch icon load request has succeeded or failed.
    virtual void OnTouchIconLoadComplete(const GURL& origin, bool successful) {}

   protected:
    virtual ~Observer() = default;
  };

  RemoteNtpService(const base::FilePath& profile_path,
                   PrefService* pref_service);
  ~RemoteNtpService() override;

  static bool IsRemoteNtpUrl(const GURL& url);

  // Add, remove, or query process IDs that are associated with RemoteNTP.
  virtual void AddRemoteNtpProcess(int process_id) {}
  virtual bool IsRemoteNtpProcess(int process_id) const;

  // Create an autocomplete controller from this RemoteNtpService.
  virtual std::unique_ptr<AutocompleteController> CreateAutocompleteController()
      const = 0;

  // Add or remove RemoteNtpService observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Invoked whenever an NTP is opened. Causes an async refresh of NTP tiles,
  // and forwards current RemoteNtpService state to all observers.
  void OnNewTabPageOpened();

  // Invoked when the NTP wants to add a custom link.
  void AddCustomTile(const GURL& tile_url, const std::u16string& tile_title);

  // Invoked when the NTP wants to remove a custom link.
  void RemoveCustomTile(const GURL& tile_url);

  // Invoked when the NTP wants to update a custom link.
  void EditCustomTile(const GURL& old_tile_url,
                      const GURL& new_tile_url,
                      const std::u16string& new_tile_title);

  // Invoked when the NTP wants the list of background collections.
  void LoadBackgroundCollections();

  // Invoked when the NTP wants a list of images for a background collection.
  void LoadBackgroundImages(const std::string& collection_id);

  // Invoked when the NTP wants to set the background image.
  void SetBackgroundImage(const std::string& collection_id,
                          rebel::mojom::BackgroundImagePtr image);

  // Invoked when the system dark mode setting has changed.
  void SetDarkModeEnabled(bool dark_mode_enabled);

  // Retrieve the current theme (might be NULL).
  const rebel::mojom::RemoteNtpThemePtr& theme() const { return theme_; }

  // Retrieve the icon cache storage.
  RemoteNtpIconStorage* icon_storage() const { return icon_storage_.get(); }

 protected:
  // Initialize the RemoteNtpService and set up observers needed to run the
  // service. Should be invoked by platform implementations once they determine
  // the service should actually run (i.e. is not in a unit test). Any of the
  // provided sub-services may be null.
  void InitializeService(
      std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Overridden from KeyedService:
  void Shutdown() override;

  virtual void FetchBackgroundCollections() {}
  virtual void FetchBackgroundImages(const std::string& collection_id) {}
  virtual void StoreBackgroundImage(const std::string& collection_id,
                                    rebel::mojom::BackgroundImagePtr image) {}
  virtual rebel::mojom::RemoteNtpThemePtr CreateTheme();

 private:
  RemoteNtpService(const RemoteNtpService&) = delete;
  RemoteNtpService& operator=(const RemoteNtpService&) = delete;

  // Overridden from ntp_tiles::MostVisitedSites::Observer:
  void OnURLsAvailable(
      const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
          sections) override;
  void OnIconMadeAvailable(const GURL& site_url) override;

  // Overridden from rebel::RemoteNtpThemeDelegate:
  void OnBackgroundCollectionsAvailable(
      rebel::RemoteNtpBackgroundCollectionList collections) override;
  void OnBackgroundImagesAvailable(
      const std::string& collection,
      rebel::RemoteNtpBackgroundImageList images) override;
  void OnThemeUpdated() override;

  // Overridden from rebel::RemoteNtpIconStorage::Delegate:
  void OnIconStored(const rebel::mojom::RemoteNtpIconPtr& icon,
                    const base::FilePath& icon_file) override;
  void OnIconEvicted(const GURL& origin,
                     const base::FilePath& icon_file) override;
  void OnIconLoadComplete(const GURL& origin, bool successful) override;

  void NotifyAboutNtpTiles();
  void NotifyAboutBackgroundCollections();
  void NotifyAboutBackgroundImages();
  void NotifyAboutTheme();

  const base::FilePath profile_path_;
  PrefService* pref_service_;

  base::ObserverList<Observer>::Unchecked observers_;

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_;
  rebel::RemoteNtpTileList ntp_tiles_;

  rebel::RemoteNtpBackgroundCollectionList background_collections_;
  rebel::RemoteNtpBackgroundImageMap background_images_;
  rebel::mojom::RemoteNtpThemePtr theme_;

  std::unique_ptr<rebel::RemoteNtpIconStorage> icon_storage_;

  base::WeakPtrFactory<RemoteNtpService> weak_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SERVICE_H_
