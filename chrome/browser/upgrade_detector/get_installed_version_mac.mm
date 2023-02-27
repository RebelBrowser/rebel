// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/common/chrome_features.h"
#include "components/version_info/version_info.h"

#include "build/branding_buildflags.h"  // Needed for REBEL_BROWSER.
#if BUILDFLAG(REBEL_BROWSER)
#include "rebel/chrome/browser/mac/sparkle_glue.h"
#endif

namespace {

InstalledAndCriticalVersion GetInstalledVersionSynchronous() {
#if BUILDFLAG(REBEL_BROWSER)
  return InstalledAndCriticalVersion(base::Version(
      base::UTF16ToASCII(rebel::CurrentlyDownloadedVersion())));
#else
  if (base::FeatureList::IsEnabled(features::kUseChromiumUpdater)) {
    return InstalledAndCriticalVersion(
        base::Version(CurrentlyInstalledVersion()));
  }
  if (!keystone_glue::KeystoneEnabled()) {
    return InstalledAndCriticalVersion(version_info::GetVersion());
  }
  return InstalledAndCriticalVersion(base::Version(
      base::UTF16ToASCII(keystone_glue::CurrentlyInstalledVersion())));
#endif
}

}  // namespace

void GetInstalledVersion(InstalledVersionCallback callback) {
  std::move(callback).Run(GetInstalledVersionSynchronous());
}
