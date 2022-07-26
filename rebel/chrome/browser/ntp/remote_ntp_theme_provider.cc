// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_theme_provider.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_factory.h"
#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_theme_delegate.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

namespace rebel {

namespace {

const char kRemoteNtpBackgroundDict[] = "remote_ntp.background";

const char kBackgroundCollectionId[] = "collection_id";
const char kBackgroundImageURL[] = "image_url";
const char kBackgroundAttributionLine1[] = "attribution_line_1";
const char kBackgroundAttributionLine2[] = "attribution_line_2";
const char kBackgroundAttributionURL[] = "attribution_url";

const char kLocalBackgroundCollectionId[] = "local_background";

const char kThemeImageFormat[] =
    "chrome-search://theme/IDR_THEME_NTP_BACKGROUND?%s";
const char kThemeAttributionFormat[] =
    "chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION?%s";

base::Value::Dict BackgroundImageDefaults() {
  base::Value::Dict defaults;

  defaults.Set(kBackgroundCollectionId, std::string());
  defaults.Set(kBackgroundImageURL, std::string());
  defaults.Set(kBackgroundAttributionLine1, std::string());
  defaults.Set(kBackgroundAttributionLine2, std::string());
  defaults.Set(kBackgroundAttributionURL, std::string());

  return defaults;
}

}  // namespace

RemoteNtpThemeProvider::RemoteNtpThemeProvider(RemoteNtpThemeDelegate* delegate,
                                               Profile* profile)
    : delegate_(delegate),
      profile_(profile),
      pref_service_(profile->GetPrefs()),
      theme_observer_(this),
      native_theme_(ui::NativeTheme::GetInstanceForNativeUi()),
      dark_mode_enabled_(false),
      background_service_observer_(this),
      background_service_(NtpBackgroundServiceFactory::GetForProfile(profile)),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)),
      chrome_colors_service_(
          chrome_colors::ChromeColorsFactory::GetForProfile(profile)),
      weak_factory_(this) {
  dark_mode_enabled_ = native_theme_->ShouldUseDarkColors();
  theme_observer_.Observe(native_theme_);

  if (background_service_) {
    background_service_observer_.Observe(background_service_);
  }
  if (theme_service_) {
    theme_service_->AddObserver(this);
  }
}

RemoteNtpThemeProvider::~RemoteNtpThemeProvider() {
  if (theme_service_) {
    theme_service_->RemoveObserver(this);
  }
}

void RemoteNtpThemeProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kRemoteNtpBackgroundDict,
                                   BackgroundImageDefaults());
}

void RemoteNtpThemeProvider::FetchBackgroundCollections() {
  if (!background_service_) {
    return;
  }

  background_service_->FetchCollectionInfo();
}

void RemoteNtpThemeProvider::FetchBackgroundImages(
    const std::string& collection_id) {
  if (!background_service_) {
    return;
  }

  background_service_->FetchCollectionImageInfo(collection_id);
}

void RemoteNtpThemeProvider::StoreBackgroundImage(
    const std::string& collection_id,
    rebel::mojom::BackgroundImagePtr image) {
  if (!background_service_) {
    return;
  }

  if (collection_id.empty()) {
    pref_service_->SetDict(kRemoteNtpBackgroundDict, BackgroundImageDefaults());
  } else {
    base::Value::Dict new_background =
        pref_service_->GetValueDict(kRemoteNtpBackgroundDict).Clone();

    new_background.Set(kBackgroundCollectionId, collection_id);
    new_background.Set(kBackgroundImageURL, image->image_url.spec());
    new_background.Set(kBackgroundAttributionLine1, image->attribution_line_1);
    new_background.Set(kBackgroundAttributionLine2, image->attribution_line_2);
    new_background.Set(kBackgroundAttributionURL,
                       image->attribution_url.spec());

    pref_service_->SetDict(kRemoteNtpBackgroundDict, std::move(new_background));
  }

  OnThemeChanged();
}

void RemoteNtpThemeProvider::SelectLocalBackgroundImage(
    content::WebContents* tab) {
  if (select_file_dialog_) {
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(tab));

  const base::FilePath directory = profile_->last_selected_directory();
  gfx::NativeWindow parent_window = tab->GetTopLevelNativeWindow();

  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  file_types.extensions.resize(1);
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("png"));
  file_types.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_UPLOAD_IMAGE_FORMAT));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(), directory,
      &file_types, 0, base::FilePath::StringType(), parent_window, nullptr);

  OnThemeChanged();
}

void RemoteNtpThemeProvider::StoreLocalBackgroundImage(bool copy_result) {
  if (!copy_result || !background_service_) {
    return;
  }

  // Add a timestamp to the URL to prevent the browser from using a cached
  // version when "Select from device" is used multiple times.
  const std::string image(rebel::kRemoteNtpLocalBackgroundUrl);
  const std::string now(std::to_string(base::Time::Now().ToTimeT()));
  const GURL image_url(image + "?ts=" + now);

  base::Value::Dict new_background =
      pref_service_->GetValueDict(kRemoteNtpBackgroundDict).Clone();

  new_background.Set(kBackgroundCollectionId, kLocalBackgroundCollectionId);
  new_background.Set(kBackgroundImageURL, image_url.spec());
  new_background.Set(kBackgroundAttributionLine1, std::string());
  new_background.Set(kBackgroundAttributionLine2, std::string());
  new_background.Set(kBackgroundAttributionURL, std::string());

  pref_service_->SetDict(kRemoteNtpBackgroundDict, std::move(new_background));

  if (delegate_) {
    delegate_->OnLocalBackgroundImageSelected();
  }
}

void RemoteNtpThemeProvider::PreviewColor(content::WebContents* tab,
                                          SkColor color) {
  if (!chrome_colors_service_) {
    return;
  }

  if (color == SK_ColorTRANSPARENT) {
    chrome_colors_service_->ApplyDefaultTheme(tab);
  } else {
    chrome_colors_service_->ApplyAutogeneratedTheme(color, tab);
  }
}

void RemoteNtpThemeProvider::RevertColor(content::WebContents* tab) {
  if (!chrome_colors_service_) {
    return;
  }

  if (tab) {
    chrome_colors_service_->RevertThemeChangesForTab(tab);
  } else {
    chrome_colors_service_->RevertThemeChanges();
  }
}

void RemoteNtpThemeProvider::CommitColor() {
  if (!chrome_colors_service_) {
    return;
  }

  chrome_colors_service_->ConfirmThemeChanges();
}

rebel::mojom::RemoteNtpThemePtr RemoteNtpThemeProvider::CreateTheme() {
  auto theme = rebel::mojom::RemoteNtpTheme::New();
  theme->dark_mode_enabled = dark_mode_enabled_;

  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);

  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service && theme_service->UsingExtensionTheme()) {
    SetExtensionThemeDetails(theme_service->GetThemeID(), theme_provider,
                             theme.get());
  } else if (theme_service && theme_service->UsingAutogeneratedTheme()) {
    const SkColor color = theme_service->GetAutogeneratedThemeColor();

    theme->color_id = chrome_colors::ChromeColorsService::GetColorId(color);
    theme->color = color;
    theme->color_dark =
        theme_provider.GetColor(ThemeProperties::COLOR_FRAME_ACTIVE);
    theme->color_light =
        theme_provider.GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  }

  const base::Value::Dict& background =
      pref_service_->GetValueDict(kRemoteNtpBackgroundDict);

  const std::string* image_url = background.FindString(kBackgroundImageURL);

  if (image_url && !image_url->empty()) {
    theme->collection_id = *background.FindString(kBackgroundCollectionId);
    theme->image_url = GURL(*image_url);
    theme->image_alignment = ThemeProperties::AlignmentToString(0);
    theme->image_tiling = ThemeProperties::TilingToString(0);
    theme->attribution_line_1 =
        *background.FindString(kBackgroundAttributionLine1);
    theme->attribution_line_2 =
        *background.FindString(kBackgroundAttributionLine2);
    theme->attribution_url =
        GURL(*background.FindString(kBackgroundAttributionURL));
    theme->attribution_image_url = GURL();
  }

  return theme;
}

void RemoteNtpThemeProvider::SetExtensionThemeDetails(
    const std::string& theme_id,
    const ui::ThemeProvider& theme_provider,
    rebel::mojom::RemoteNtpTheme* theme) {
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);

  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(theme_id);
  if (!extension) {
    return;
  }

  if (theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    theme->image_url =
        GURL(base::StringPrintf(kThemeImageFormat, theme_id.c_str()));

    const int alignment = theme_provider.GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
    theme->image_alignment = ThemeProperties::AlignmentToString(alignment);

    const int tiling = theme_provider.GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_TILING);
    theme->image_tiling = ThemeProperties::TilingToString(tiling);

    if (theme_provider.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION)) {
      theme->attribution_image_url =
          GURL(base::StringPrintf(kThemeAttributionFormat, theme_id.c_str()));
    }
  }
}

void RemoteNtpThemeProvider::OnNativeThemeUpdated(
    ui::NativeTheme* native_theme) {
  dark_mode_enabled_ = native_theme_->ShouldUseDarkColors();
  OnThemeChanged();
}

void RemoteNtpThemeProvider::FileSelected(const base::FilePath& path,
                                          int index,
                                          void* params) {
  profile_->set_last_selected_directory(path.DirName());
  select_file_dialog_ = nullptr;

  base::FilePath destination_path =
      profile_->GetPath().AppendASCII(rebel::kRemoteNtpLocalBackgroundPath);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&base::CopyFile, path, destination_path),
      base::BindOnce(&RemoteNtpThemeProvider::StoreLocalBackgroundImage,
                     weak_factory_.GetWeakPtr()));
}

void RemoteNtpThemeProvider::FileSelectionCanceled(void* params) {
  select_file_dialog_ = nullptr;
}

void RemoteNtpThemeProvider::OnCollectionInfoAvailable() {
  const auto& collection_info = background_service_->collection_info();
  if (collection_info.empty()) {
    return;
  }

  rebel::RemoteNtpBackgroundCollectionList collections;

  for (const CollectionInfo& info : collection_info) {
    auto mojo_collection = rebel::mojom::BackgroundCollection::New(
        info.collection_id, info.collection_name, info.preview_image_url);

    collections.push_back(std::move(mojo_collection));
  }

  if (delegate_) {
    delegate_->OnBackgroundCollectionsAvailable(std::move(collections));
  }
}

void RemoteNtpThemeProvider::OnCollectionImagesAvailable() {
  const auto& collection_images = background_service_->collection_images();
  if (collection_images.empty()) {
    return;
  }

  const std::string& collection_id = collection_images[0].collection_id;
  rebel::RemoteNtpBackgroundImageList images;

  for (const CollectionImage& image : collection_images) {
    std::string attribution_line_1;
    std::string attribution_line_2;

    if (!image.attribution.empty()) {
      attribution_line_1 = image.attribution[0];

      if (image.attribution.size() > 1) {
        attribution_line_2 = image.attribution[1];
      }
    }

    auto mojo_image = rebel::mojom::BackgroundImage::New(
        image.image_url, image.thumbnail_image_url, attribution_line_1,
        attribution_line_2, image.attribution_action_url);

    images.push_back(std::move(mojo_image));
  }

  if (delegate_) {
    delegate_->OnBackgroundImagesAvailable(collection_id, std::move(images));
  }
}

void RemoteNtpThemeProvider::OnNextCollectionImageAvailable() {}

void RemoteNtpThemeProvider::OnNtpBackgroundServiceShuttingDown() {
  background_service_observer_.Reset();
  background_service_ = nullptr;
}

void RemoteNtpThemeProvider::OnThemeChanged() {
  if (delegate_) {
    delegate_->OnThemeUpdated();
  }
}

void RemoteNtpThemeProvider::SetNativeThemeForTesting(ui::NativeTheme* theme) {
  theme_observer_.Reset();
  native_theme_ = theme;
  theme_observer_.Observe(native_theme_);
}

}  // namespace rebel
