// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_STORAGE_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_STORAGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

class PrefRegistrySimple;
class PrefService;
class SkBitmap;

class RemoteNtpTest;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageDecoder;
}  // namespace image_fetcher

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace rebel {

struct CachedIcon {
  CachedIcon();
  CachedIcon(const CachedIcon&);
  CachedIcon& operator=(const CachedIcon&);

  // The URL of the domain's preferred icon.
  GURL icon_url;

  // The local absolute file path where the icon is cached.
  base::FilePath icon_file;

  // Type of the icon.
  rebel::mojom::RemoteNtpIconType icon_type{
      rebel::mojom::RemoteNtpIconType::Unknown};

  // Size of the icon (assuming square icons).
  int icon_size{-1};

  // Time the icon was fetched from the remove domain.
  base::Time icon_fetch_time;

  // Last time a renderer found the domain's icon.
  base::Time last_visit_time;

  // Last time the RemoteNTP requested the icon for display.
  base::Time last_request_time;
};

using CachedIconMap = std::map<const GURL, CachedIcon>;

using CachedIconFoundCallback = base::OnceCallback<void(SkBitmap)>;

// Backend storage to manage icons parsed by the renderer. Stores the icons in
// the user's profile. Handles limiting the size of the icon storage.
class RemoteNtpIconStorage {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when an icon has been successfully stored or updated.
    virtual void OnIconStored(const rebel::mojom::RemoteNtpIconPtr& icon,
                              const base::FilePath& icon_file) = 0;

    // Invoked when an icon has been removed from storage.
    virtual void OnIconEvicted(const GURL& origin,
                               const base::FilePath& icon_file) = 0;

    // Invoked when an icon load request has succeeded or failed.
    virtual void OnIconLoadComplete(const GURL& origin, bool successful) = 0;
  };

  RemoteNtpIconStorage(
      Delegate* delegate,
      const base::FilePath& profile_path,
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~RemoteNtpIconStorage();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  void SerializeCachedIcons(base::Value::List& into_value) const;

  // When an icon is parsed by the renderer, decide if that icon should be
  // fetched or ignored as a duplicate. If it is a duplicate, update its
  // metadata and notify the delegate.
  void FetchIconIfNeeded(rebel::mojom::RemoteNtpIconPtr icon);

  // Read the raw data of the cached icon for the requested origin. When read,
  // the image is resized to the given |size| (if non-zero) and |callback| is
  // triggered on the same sequence on which this method was invoked. If the
  // icon could not be found or read, |callback| is triggered with an empty
  // image, and the origin is removed from the cache.
  void GetIconForOrigin(const GURL& origin,
                        int size,
                        CachedIconFoundCallback callback);

  // Remove an icon from the cache for the requested origin.
  bool DeleteIconForOrigin(const GURL& origin);

 private:
  RemoteNtpIconStorage(const RemoteNtpIconStorage&) = delete;
  RemoteNtpIconStorage& operator=(const RemoteNtpIconStorage&) = delete;

  friend class ::RemoteNtpTest;

  // Retrieve an iterator to the cached icon metadata for the requested origin.
  CachedIconMap::iterator FindCachedIconDataForOrigin(const GURL& origin);

  // Create and fetch a spec-compliant default touch icon for the given origin.
  // According to the spec, an origin does not need to embed a <link> element
  // if they place a file named "apple-touch-icon.png" at the origin's root.
  void FetchSpecCompliantDefaultIcon(const GURL& origin, base::Time visit_time);

  // Create the resource request and URL loader to fetch an icon.
  void FetchIcon(rebel::mojom::RemoteNtpIconPtr icon, base::Time visit_time);

  // When a remote icon has been fetched, decode the raw data to a gfx::Image.
  void OnIconFetchComplete(rebel::mojom::RemoteNtpIconPtr icon,
                           base::Time visit_time,
                           std::unique_ptr<std::string> icon_data);

  // When an icon has been decoded, convert it to PNG and store it on disk.
  void OnIconDecodeComplete(rebel::mojom::RemoteNtpIconPtr icon,
                            base::Time visit_time,
                            const gfx::Image& image);

  // When an icon has been stored on disk, store/update its metadata in memory.
  // If the cache has become full, perform cache eviction. Notify the delegate.
  void OnIconStorageComplete(rebel::mojom::RemoteNtpIconPtr icon,
                             base::Time visit_time,
                             int icon_size,
                             base::FilePath icon_file);

  // When an icon has been removed from disk, notify the delegate.
  void OnIconDeletionComplete(GURL origin, base::FilePath icon_file);

  // When an icon has been read from disk, decode and resize it, and invoke the
  // callback. Handle errors if the icon could not be read for any reason.
  void OnIconReadComplete(CachedIconFoundCallback callback,
                          GURL origin,
                          int size,
                          std::string icon_data);

  // When an icon failed to be read from disk or decoded, remove the icon from
  // cache, ensure it is removed from disk, and notify the delegate.
  void OnIconReadFailed(CachedIconFoundCallback callback, GURL origin);

  // If the storage cache is full, remove icons until it is no longer full.
  void RunEvictionIfFull();

  // Find a candidate icon to evict from cache. Prefer to remove an icon which
  // was least recently visited or requested by the RemoteNTP for display.
  CachedIconMap::value_type FindIconToEvict();

  void InitializeFromPrefs();
  void SerializeToPrefs() const;

  static void SetCacheSizeLimitForTesting(size_t limit) {
    cache_size_limit_for_testing_ = limit;
  }

  static size_t cache_size_limit_for_testing_;

  raw_ptr<Delegate> delegate_;

  const base::FilePath storage_path_;
  raw_ptr<PrefService> pref_service_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;
  CachedIconMap cached_icons_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RemoteNtpIconStorage> weak_ptr_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_STORAGE_H_
