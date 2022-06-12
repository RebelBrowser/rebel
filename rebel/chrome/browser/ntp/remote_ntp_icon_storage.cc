// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"

#include <vector>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"

#if BUILDFLAG(IS_IOS)
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#else
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#endif

namespace rebel {

namespace {

constexpr const char kRemoteNtpIconsPref[] = "remote_ntp.icons";
constexpr const char kHostOriginPref[] = "host_origin";
constexpr const char kIconUrlPref[] = "icon_url";
constexpr const char kIconFilePref[] = "icon_file";
constexpr const char kIconTypePref[] = "icon_type";
constexpr const char kIconSizePref[] = "icon_size";
constexpr const char kIconFetchTime[] = "icon_fetch_time";
constexpr const char kLastVisitTimePref[] = "last_visit_time";
constexpr const char kLastRequestTimePref[] = "last_request_time";

constexpr const base::FilePath::CharType kRemoteNtpIconPath[] =
    FILE_PATH_LITERAL("RemoteNTP Icons");

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("remote_ntp_icon_storage", R"(
        semantics {
          sender: "RemoteNTP Icon Storage"
          description:
            "Rebel retrieves large touch icons for the RemoteNTP."
          trigger:
            "Triggered when the browser has parsed the URL for a large touch "
            "icon that it does not already have."
          data: "The URL for which to retrieve an icon."
          destination: REBEL_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");

constexpr const auto kIconRefreshInterval = base::Days(7);
constexpr const size_t kIconCacheMaxSize = 100;

constexpr const gfx::Size kTouchIconExpectedSize(180, 180);
constexpr const int kMinimumFaviconSize = 64;

std::unique_ptr<image_fetcher::ImageDecoder> CreateImageDecoder() {
#if BUILDFLAG(IS_IOS)
  return image_fetcher::CreateIOSImageDecoder();
#else
  return std::make_unique<ImageDecoderImpl>();
#endif
}

std::string RandomFileName() {
  static constexpr size_t kFileNameSize = 20;

  auto random_bytes = base::RandBytesAsVector(kFileNameSize);
  return base::HexEncode(random_bytes.data(), random_bytes.size());
}

void CreateIconDirectory(const base::FilePath& storage_path) {
  if (base::DirectoryExists(storage_path)) {
    return;
  }

  if (!base::CreateDirectory(storage_path)) {
    NOTREACHED() << "Could not create directory: " << storage_path;
  }
}

std::string EncodeIconAsPNG(const gfx::Image& image) {
  std::vector<unsigned char> encoded;

  if (gfx::PNGCodec::EncodeBGRASkBitmap(image.AsBitmap(), false, &encoded)) {
    return std::string(encoded.begin(), encoded.end());
  }

  return std::string();
}

base::FilePath SaveIconToDisk(const base::FilePath& storage_path,
                              std::string icon_data) {
  base::FilePath icon_file = storage_path.AppendASCII(RandomFileName());

  if (!base::WriteFile(icon_file, std::move(icon_data))) {
    NOTREACHED() << "Could not create file: " << icon_file;
    return base::FilePath();
  }

  return icon_file;
}

base::FilePath DeleteIconFromDisk(base::FilePath&& icon_file) {
  if (!base::DeleteFile(icon_file)) {
    return base::FilePath();
  }

  return std::move(icon_file);
}

std::string ReadIconFromDisk(base::FilePath icon_file) {
  std::string icon_data;
  base::ReadFileToString(icon_file, &icon_data);

  return icon_data;
}

bool ShouldPreferCachedIcon(const rebel::CachedIcon& old_icon,
                            const rebel::mojom::RemoteNtpIcon& new_icon,
                            const base::Time& now) {
  // If the icon metadata changed, use the preferred icon type or larger icon.
  if (old_icon.icon_type < new_icon.icon_type) {
    return false;
  }

  if ((old_icon.icon_type == new_icon.icon_type) &&
      (old_icon.icon_size < new_icon.icon_size)) {
    return false;
  }

  // If the image was fetched a long time ago, refresh it.
  if ((now - old_icon.icon_fetch_time) > kIconRefreshInterval) {
    return false;
  }

  return true;
}

}  // namespace

CachedIcon::CachedIcon() = default;
CachedIcon::CachedIcon(const CachedIcon&) = default;
CachedIcon& CachedIcon::operator=(const CachedIcon&) = default;

size_t RemoteNtpIconStorage::cache_size_limit_for_testing_ = 0;

RemoteNtpIconStorage::RemoteNtpIconStorage(
    Delegate* delegate,
    const base::FilePath& profile_path,
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(delegate),
      storage_path_(profile_path.Append(kRemoteNtpIconPath)),
      pref_service_(pref_service),
      url_loader_factory_(std::move(url_loader_factory)),
      image_decoder_(CreateImageDecoder()),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  InitializeFromPrefs();

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(CreateIconDirectory, storage_path_));
}

RemoteNtpIconStorage::~RemoteNtpIconStorage() = default;

// static
void RemoteNtpIconStorage::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kRemoteNtpIconsPref);
}

void RemoteNtpIconStorage::FetchIconIfNeeded(
    rebel::mojom::RemoteNtpIconPtr icon) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto visit_time = base::Time::Now();

  auto it = cached_icons_.find(icon->host_origin);

  if ((it != cached_icons_.end()) &&
      ShouldPreferCachedIcon(it->second, *icon, visit_time)) {
    it->second.last_visit_time = std::move(visit_time);
    SerializeToPrefs();

    delegate_->OnIconStored(icon, it->second.icon_file);
    return;
  }

  if (icon->icon_url.is_empty()) {
    FetchSpecCompliantDefaultIcon(icon->host_origin, std::move(visit_time));
  } else {
    FetchIcon(std::move(icon), std::move(visit_time));
  }
}

void RemoteNtpIconStorage::GetIconForOrigin(const GURL& origin,
                                            int size,
                                            CachedIconFoundCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = FindCachedIconDataForOrigin(origin);

  if (it == cached_icons_.end()) {
    delegate_->OnIconLoadComplete(origin, false);
    std::move(callback).Run(SkBitmap());
    return;
  }

  it->second.last_request_time = base::Time::Now();
  SerializeToPrefs();

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(ReadIconFromDisk, it->second.icon_file),
      base::BindOnce(&RemoteNtpIconStorage::OnIconReadComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     it->first, size));
}

bool RemoteNtpIconStorage::DeleteIconForOrigin(const GURL& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = cached_icons_.find(origin);
  if (it == cached_icons_.end()) {
    return false;
  }

  base::FilePath icon_file = std::move(it->second).icon_file;
  cached_icons_.erase(it);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(DeleteIconFromDisk, std::move(icon_file)),
      base::BindOnce(&RemoteNtpIconStorage::OnIconDeletionComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(origin)));

  return true;
}

CachedIconMap::iterator RemoteNtpIconStorage::FindCachedIconDataForOrigin(
    const GURL& origin) {
  rebel::CachedIconMap::iterator it;

  // Rather than using std::map::find, use approximate equality to match domains
  // with slight component differences. For example, a domain may contain www,
  // but the user may not have included that component in the NTP tile URL.
  for (it = cached_icons_.begin(); it != cached_icons_.end(); ++it) {
    if (origin.DomainIs(it->first.host_piece()) ||
        it->first.DomainIs(origin.host_piece())) {
      break;
    }
  }

  return it;
}

void RemoteNtpIconStorage::FetchSpecCompliantDefaultIcon(
    const GURL& origin,
    base::Time visit_time) {
  GURL::Replacements replacements;
  replacements.SetPathStr("apple-touch-icon.png");
  replacements.ClearQuery();
  replacements.ClearRef();

  auto icon = rebel::mojom::RemoteNtpIcon::New();
  icon->host_origin = origin;
  icon->icon_url = origin.ReplaceComponents(replacements);
  icon->icon_type = rebel::mojom::RemoteNtpIconType::Touch;

  FetchIcon(std::move(icon), std::move(visit_time));
}

void RemoteNtpIconStorage::FetchIcon(rebel::mojom::RemoteNtpIconPtr icon,
                                     base::Time visit_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto origin = url::Origin::Create(icon->icon_url);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = icon->icon_url;
  resource_request->site_for_cookies = net::SiteForCookies::FromOrigin(origin);
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(origin);

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotationTag);
  url_loader_->SetRetryOptions(
      1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RemoteNtpIconStorage::OnIconFetchComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(icon),
                     std::move(visit_time)));
}

void RemoteNtpIconStorage::OnIconFetchComplete(
    rebel::mojom::RemoteNtpIconPtr icon,
    base::Time visit_time,
    std::unique_ptr<std::string> icon_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_.reset();

  if (!icon_data || icon_data->empty()) {
    return;
  }

  image_decoder_->DecodeImage(
      *icon_data, kTouchIconExpectedSize,
      nullptr,  // TODO(tflynn): Use a cached process for image decoding.
      base::BindOnce(&RemoteNtpIconStorage::OnIconDecodeComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(icon),
                     std::move(visit_time)));
}

void RemoteNtpIconStorage::OnIconDecodeComplete(
    rebel::mojom::RemoteNtpIconPtr icon,
    base::Time visit_time,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    return;
  }

  std::string icon_data = EncodeIconAsPNG(image);
  if (icon_data.empty()) {
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(SaveIconToDisk, storage_path_, std::move(icon_data)),
      base::BindOnce(&RemoteNtpIconStorage::OnIconStorageComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(icon),
                     std::move(visit_time), image.Width()));
}

void RemoteNtpIconStorage::OnIconStorageComplete(
    rebel::mojom::RemoteNtpIconPtr icon,
    base::Time visit_time,
    int icon_size,
    base::FilePath icon_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_.reset();

  if (icon_file.empty()) {
    return;
  }

  // If the icon was a favicon, issue a new request for the spec-compliant
  // default touch icon. Discard the favicon if it is too small.
  if (icon->icon_type == rebel::mojom::RemoteNtpIconType::Favicon) {
    FetchSpecCompliantDefaultIcon(icon->host_origin, visit_time);

    if (icon_size < kMinimumFaviconSize) {
      return;
    }
  }

  auto it = cached_icons_.find(icon->host_origin);
  auto fetch_time = base::Time::Now();

  // If this is a new icon, make room for it in the cache. If this is an updated
  // icon, delete the old file from the cache.
  if (it == cached_icons_.end()) {
    RunEvictionIfFull();

    auto result = cached_icons_.emplace(icon->host_origin, CachedIcon{});
    it = result.first;
  } else {
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(DeleteIconFromDisk, std::move(it->second.icon_file)),
        base::BindOnce(&RemoteNtpIconStorage::OnIconDeletionComplete,
                       weak_ptr_factory_.GetWeakPtr(), icon->host_origin));
  }

  it->second.icon_url = icon->icon_url;
  it->second.icon_file = std::move(icon_file);
  it->second.icon_type = icon->icon_type;
  it->second.icon_size = icon_size;
  it->second.icon_fetch_time = std::move(fetch_time);
  it->second.last_visit_time = std::move(visit_time);

  SerializeToPrefs();

  delegate_->OnIconStored(icon, it->second.icon_file);
}

void RemoteNtpIconStorage::OnIconDeletionComplete(GURL origin,
                                                  base::FilePath icon_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (icon_file.empty()) {
    return;
  }

  delegate_->OnIconEvicted(origin, icon_file);
}

void RemoteNtpIconStorage::OnIconReadComplete(CachedIconFoundCallback callback,
                                              GURL origin,
                                              int size,
                                              std::string icon_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (icon_data.empty()) {
    OnIconReadFailed(std::move(callback), std::move(origin));
    return;
  }

  auto data =
      base::MakeRefCounted<base::RefCountedString>(std::move(icon_data));
  SkBitmap icon;

  if (!gfx::PNGCodec::Decode(data.get()->front(), data.get()->size(), &icon)) {
    OnIconReadFailed(std::move(callback), std::move(origin));
    return;
  }

  if (size > 0) {
    icon = skia::ImageOperations::Resize(
        icon, skia::ImageOperations::RESIZE_BEST, size, size);
  }

  delegate_->OnIconLoadComplete(origin, true);
  std::move(callback).Run(std::move(icon));
}

void RemoteNtpIconStorage::OnIconReadFailed(CachedIconFoundCallback callback,
                                            GURL origin) {
  delegate_->OnIconLoadComplete(origin, false);
  std::move(callback).Run(SkBitmap());

  auto it = cached_icons_.find(origin);
  DCHECK(it != cached_icons_.end());

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(DeleteIconFromDisk, std::move(it->second.icon_file)),
      base::BindOnce(&RemoteNtpIconStorage::OnIconDeletionComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(origin)));

  cached_icons_.erase(it);
  SerializeToPrefs();
}

void RemoteNtpIconStorage::RunEvictionIfFull() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static const size_t cache_size_limit = (cache_size_limit_for_testing_ == 0)
                                             ? kIconCacheMaxSize
                                             : cache_size_limit_for_testing_;

  if (cached_icons_.size() < cache_size_limit) {
    return;
  }

  while (cached_icons_.size() >= cache_size_limit) {
    rebel::CachedIconMap::value_type icon_to_evict = FindIconToEvict();
    cached_icons_.erase(icon_to_evict.first);

    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(DeleteIconFromDisk,
                       std::move(icon_to_evict.second.icon_file)),
        base::BindOnce(&RemoteNtpIconStorage::OnIconDeletionComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(icon_to_evict.first)));
  }
}

rebel::CachedIconMap::value_type RemoteNtpIconStorage::FindIconToEvict() {
  // First consider icons that were never requested by the RemoteNTP - remove
  // the icon for the least recently visited origin.
  auto candidate = cached_icons_.end();

  for (auto it = cached_icons_.begin(); it != cached_icons_.end(); ++it) {
    if (!it->second.last_request_time.is_null()) {
      continue;
    }

    if (candidate == cached_icons_.end()) {
      candidate = it;
    } else if (it->second.last_visit_time < candidate->second.last_visit_time) {
      candidate = it;
    }
  }

  if (candidate != cached_icons_.end()) {
    return *candidate;
  }

  // Otherwise evict the icon least recently requested by the RemoteNTP.
  candidate = cached_icons_.begin();

  for (auto it = cached_icons_.begin(); it != cached_icons_.end(); ++it) {
    if (it->second.last_request_time < candidate->second.last_request_time) {
      candidate = it;
    }
  }

  return *candidate;
}

void RemoteNtpIconStorage::InitializeFromPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::List& prefs = pref_service_->GetList(kRemoteNtpIconsPref);

  for (const base::Value& icon_value : prefs) {
    if (!icon_value.is_dict()) {
      continue;
    }

    const auto& icon = icon_value.GetDict();
    const std::string* host_origin = icon.FindString(kHostOriginPref);
    const std::string* icon_url = icon.FindString(kIconUrlPref);
    auto icon_file = base::ValueToFilePath(icon.Find(kIconFilePref));

    if (!host_origin || !icon_url || !icon_file) {
      continue;
    }

#if BUILDFLAG(IS_IOS)
    // On iOS, the browser state path changes after updates. The user's profile
    // directory is migrated, but the paths stored in prefs will be stale.
    icon_file = storage_path_.Append(icon_file->BaseName());
#endif

    auto icon_type = icon.FindInt(kIconTypePref);
    auto icon_size = icon.FindInt(kIconSizePref);
    auto icon_fetch_time = base::ValueToTime(icon.Find(kIconFetchTime));
    auto last_visit_time = base::ValueToTime(icon.Find(kLastVisitTimePref));
    auto last_request_time = base::ValueToTime(icon.Find(kLastRequestTimePref));

    CachedIcon cached_icon;
    cached_icon.icon_url = GURL(*icon_url);
    cached_icon.icon_file = icon_file.value();
    cached_icon.icon_type =
        static_cast<rebel::mojom::RemoteNtpIconType>(icon_type.value_or(0));
    cached_icon.icon_size = icon_size.value_or(-1);
    cached_icon.icon_fetch_time = icon_fetch_time.value_or(base::Time());
    cached_icon.last_visit_time = last_visit_time.value_or(base::Time());
    cached_icon.last_request_time = last_request_time.value_or(base::Time());

    cached_icons_.emplace(GURL(*host_origin), std::move(cached_icon));
  }

#if BUILDFLAG(IS_IOS)
  SerializeToPrefs();
#endif
}

void RemoteNtpIconStorage::SerializeCachedIcons(
    base::Value::List& into_value) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& it : cached_icons_) {
    base::Value::Dict icon;

    icon.Set(kHostOriginPref, it.first.spec());
    icon.Set(kIconUrlPref, it.second.icon_url.spec());
    icon.Set(kIconFilePref, base::FilePathToValue(it.second.icon_file));
    icon.Set(kIconTypePref, static_cast<int>(it.second.icon_type));
    icon.Set(kIconSizePref, it.second.icon_size);
    icon.Set(kIconFetchTime, base::TimeToValue(it.second.icon_fetch_time));
    icon.Set(kLastVisitTimePref, base::TimeToValue(it.second.last_visit_time));
    icon.Set(kLastRequestTimePref,
             base::TimeToValue(it.second.last_request_time));

    into_value.Append(std::move(icon));
  }
}

void RemoteNtpIconStorage::SerializeToPrefs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value::List prefs;
  SerializeCachedIcons(prefs);

  pref_service_->SetList(kRemoteNtpIconsPref, std::move(prefs));
}

}  // namespace rebel
