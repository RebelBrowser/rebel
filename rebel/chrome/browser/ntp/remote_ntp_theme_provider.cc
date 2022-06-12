// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_theme_provider.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/new_tab_page/chrome_colors/chrome_colors_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/theme_provider.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_theme_delegate.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

namespace rebel {

namespace {

constexpr const char kThemeImageFormat[] =
    "chrome-search://theme/IDR_THEME_NTP_BACKGROUND?%s";
constexpr const char kThemeAttributionFormat[] =
    "chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION?%s";

}  // namespace

RemoteNtpThemeProvider::RemoteNtpThemeProvider(RemoteNtpThemeDelegate* delegate,
                                               Profile* profile)
    : delegate_(delegate),
      profile_(profile),
      theme_observer_(this),
      native_theme_(ui::NativeTheme::GetInstanceForNativeUi()),
      dark_mode_enabled_(false),
      custom_background_service_observer_(this),
      custom_background_service_(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile_)),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)) {
  dark_mode_enabled_ = native_theme_->ShouldUseDarkColors();
  theme_observer_.Observe(native_theme_);

  if (custom_background_service_) {
    custom_background_service_observer_.Observe(custom_background_service_);
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

rebel::mojom::RemoteNtpThemePtr RemoteNtpThemeProvider::CreateTheme() {
  auto theme = rebel::mojom::RemoteNtpTheme::New();
  theme->dark_mode_enabled = dark_mode_enabled_;

  if (theme_service_ && theme_service_->UsingExtensionTheme()) {
    SetExtensionThemeDetails(theme_service_->GetThemeID(), theme.get());
  } else if (custom_background_service_) {
    if (auto background = custom_background_service_->GetCustomBackground()) {
      theme->image_url = background->custom_background_url;
      theme->image_alignment = ThemeProperties::AlignmentToString(0);
      theme->image_tiling = ThemeProperties::TilingToString(0);
      theme->attribution_line_1 =
          background->custom_background_attribution_line_1;
      theme->attribution_line_2 =
          background->custom_background_attribution_line_2;
      theme->attribution_url =
          background->custom_background_attribution_action_url;
    }
  }

  return theme;
}

void RemoteNtpThemeProvider::SetExtensionThemeDetails(
    const std::string& theme_id,
    rebel::mojom::RemoteNtpTheme* theme) {
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);

  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(theme_id);
  if (!extension) {
    return;
  }

  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);

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

void RemoteNtpThemeProvider::OnCustomBackgroundImageUpdated() {
  OnThemeChanged();
}

void RemoteNtpThemeProvider::OnNtpCustomBackgroundServiceShuttingDown() {
  custom_background_service_observer_.Reset();
  custom_background_service_ = nullptr;
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
