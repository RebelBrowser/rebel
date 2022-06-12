// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_source.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/favicon_base/favicon_util.h"
#include "content/public/browser/browser_context.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_icon_storage.h"
#include "rebel/chrome/browser/ntp/remote_ntp_offline_resources.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_impl.h"
#include "rebel/chrome/common/ntp/remote_ntp_icon_util.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

namespace rebel {

namespace {

std::string ReadBackgroundImageData(const base::FilePath& profile_path) {
  std::string data_string;

  base::ReadFileToString(
      profile_path.AppendASCII(rebel::kRemoteNtpLocalBackgroundPath),
      &data_string);

  return data_string;
}

void ServeRawImage(content::URLDataSource::GotDataCallback callback,
                   std::string image_data) {
  auto data =
      base::MakeRefCounted<base::RefCountedString>(std::move(image_data));
  std::move(callback).Run(std::move(data));
}

void ServeBitmap(content::URLDataSource::GotDataCallback callback,
                 SkBitmap icon) {
  std::vector<unsigned char> data;

  if (gfx::PNGCodec::EncodeBGRASkBitmap(icon, false, &data)) {
    std::move(callback).Run(base::RefCountedBytes::TakeVector(&data));
  } else {
    std::move(callback).Run(nullptr);
  }
}

}  // namespace

RemoteNtpSource::RemoteNtpSource(Profile* profile) : profile_(profile) {}
RemoteNtpSource::~RemoteNtpSource() = default;

std::string RemoteNtpSource::GetSource() {
  return rebel::kRemoteNtpOfflineHost;
}

void RemoteNtpSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  base::StringPiece file_path = url.path_piece().substr(1);

  float scale = 1.0f;
  auto scale_factor = ui::GetSupportedResourceScaleFactor(scale);

  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(profile_);
  if (!remote_ntp_service || !remote_ntp_service->icon_storage()) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (file_path == rebel::kRemoteNtpLocalBackgroundPath) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&ReadBackgroundImageData, profile_->GetPath()),
        base::BindOnce(&ServeRawImage, std::move(callback)));

    return;
  } else if (file_path == rebel::kRemoteNtpIconPath) {
    const rebel::RemoteNtpIcon parsed = rebel::ParseIconFromURL(url);

    if (parsed.url.is_valid()) {
      remote_ntp_service->icon_storage()->GetIconForOrigin(
          parsed.url.GetWithEmptyPath(), parsed.icon_size,
          base::BindOnce(&ServeBitmap, std::move(callback)));
    } else {
      std::move(callback).Run(nullptr);
    }

    return;
  }

  for (auto const& resource : kRemoteNtpOfflineResources) {
    if (file_path == resource.file_path) {
      scoped_refptr<base::RefCountedMemory> response(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
              resource.identifier, scale_factor));

      std::move(callback).Run(response.get());
      return;
    }
  }

  std::move(callback).Run(nullptr);
}

std::string RemoteNtpSource::GetMimeType(const GURL& url) {
  base::StringPiece file_path = url.path_piece().substr(1);

  if (file_path.find(rebel::kRemoteNtpLocalBackgroundPath) == 0) {
    return "image/jpg";
  } else if (file_path.find(kRemoteNtpIconPath) == 0) {
    return "image/png";
  }

  for (auto const& resource : kRemoteNtpOfflineResources) {
    if (file_path == resource.file_path) {
      return resource.mime_type;
    }
  }

  return std::string();
}

bool RemoteNtpSource::AllowCaching() {
  // RemoteNTP builds use URL cache busting on every file *except* index.html.
  // So if a cached index.html is loaded, it may include an old source that no
  // longer exists. Disallow caching to prevent this.
  return false;
}

bool RemoteNtpSource::ShouldAddContentSecurityPolicy() {
  // TODO: Embed CSP links in the RemoteNTP.
  return false;
}

bool RemoteNtpSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  if (!rebel::RemoteNtpServiceImpl::ShouldServiceRequest(url, browser_context,
                                                         render_process_id)) {
    return false;
  }

  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    base::StringPiece file_path = url.path_piece().substr(1);

    if ((file_path == rebel::kRemoteNtpLocalBackgroundPath) ||
        (file_path == kRemoteNtpIconPath)) {
      return true;
    }

    for (auto const& resource : kRemoteNtpOfflineResources) {
      if (file_path == resource.file_path) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace rebel
