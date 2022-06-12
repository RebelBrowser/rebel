// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_PROVIDER_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_PROVIDER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"

class PrefRegistrySimple;
class PrefService;
class Profile;
class ThemeService;

namespace base {
class FilePath;
}  // namespace base

namespace chrome_colors {
class ChromeColorsService;
}  // namespace chrome_colors

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class ThemeProvider;
}  // namespace ui

namespace rebel {

class RemoteNtpThemeDelegate;

class RemoteNtpThemeProvider : public NtpBackgroundServiceObserver,
                               public ThemeServiceObserver,
                               public ui::NativeThemeObserver,
                               public ui::SelectFileDialog::Listener {
 public:
  RemoteNtpThemeProvider(RemoteNtpThemeDelegate* delegate, Profile* profile);
  ~RemoteNtpThemeProvider() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void FetchBackgroundCollections();
  void FetchBackgroundImages(const std::string& collection_id);
  void StoreBackgroundImage(const std::string& collection_id,
                            rebel::mojom::BackgroundImagePtr image);
  void SelectLocalBackgroundImage(content::WebContents* tab);

  void PreviewColor(content::WebContents* tab, SkColor color);
  void RevertColor(content::WebContents* tab);
  void CommitColor();

  rebel::mojom::RemoteNtpThemePtr CreateTheme();

  // Used only for testing.
  void AddBackgroundImageForTesting(const GURL& image_url);
  void SetNativeThemeForTesting(ui::NativeTheme* theme);

 private:
  RemoteNtpThemeProvider(const RemoteNtpThemeProvider&) = delete;
  RemoteNtpThemeProvider& operator=(const RemoteNtpThemeProvider&) = delete;

  void SetExtensionThemeDetails(const std::string& theme_id,
                                const ui::ThemeProvider& theme_provider,
                                rebel::mojom::RemoteNtpTheme* theme);
  void StoreLocalBackgroundImage(bool copy_result);

  // Overridden from ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* native_theme) override;

  // Overridden from ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  // Overridden from NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // Overridden from ThemeServiceObserver:
  void OnThemeChanged() override;

  RemoteNtpThemeDelegate* delegate_;

  Profile* const profile_;
  PrefService* pref_service_;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observer_;
  ui::NativeTheme* native_theme_;
  bool dark_mode_enabled_;

  base::ScopedObservation<NtpBackgroundService, NtpBackgroundServiceObserver>
      background_service_observer_;
  NtpBackgroundService* background_service_;

  ThemeService* theme_service_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  chrome_colors::ChromeColorsService* chrome_colors_service_;

  base::WeakPtrFactory<RemoteNtpThemeProvider> weak_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_PROVIDER_H_
