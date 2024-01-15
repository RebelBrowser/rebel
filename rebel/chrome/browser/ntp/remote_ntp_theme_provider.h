// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_PROVIDER_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_PROVIDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"

class Profile;
class ThemeService;

class RemoteNtpThemeTest;

namespace rebel {

class RemoteNtpThemeDelegate;

class RemoteNtpThemeProvider : public NtpCustomBackgroundServiceObserver,
                               public ThemeServiceObserver,
                               public ui::NativeThemeObserver {
 public:
  RemoteNtpThemeProvider(RemoteNtpThemeDelegate* delegate, Profile* profile);
  ~RemoteNtpThemeProvider() override;

  rebel::mojom::RemoteNtpThemePtr CreateTheme();

 private:
  RemoteNtpThemeProvider(const RemoteNtpThemeProvider&) = delete;
  RemoteNtpThemeProvider& operator=(const RemoteNtpThemeProvider&) = delete;

  friend class ::RemoteNtpThemeTest;

  void SetExtensionThemeDetails(const std::string& theme_id,
                                rebel::mojom::RemoteNtpTheme* theme);

  // Overridden from ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* native_theme) override;

  // Overridden from NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;
  void OnNtpCustomBackgroundServiceShuttingDown() override;

  // Overridden from ThemeServiceObserver:
  void OnThemeChanged() override;

  void SetNativeThemeForTesting(ui::NativeTheme* theme);

  raw_ptr<RemoteNtpThemeDelegate> delegate_;

  raw_ptr<Profile> profile_;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observer_;
  raw_ptr<ui::NativeTheme> native_theme_;
  bool dark_mode_enabled_;

  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      custom_background_service_observer_;
  raw_ptr<NtpCustomBackgroundService> custom_background_service_;

  raw_ptr<ThemeService> theme_service_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_PROVIDER_H_
