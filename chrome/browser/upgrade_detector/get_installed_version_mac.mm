// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/updater/browser_updater_client_util.h"

#include "build/branding_buildflags.h"  // Needed for REBEL_BROWSER.
#if BUILDFLAG(REBEL_BROWSER)
#include "base/strings/utf_string_conversions.h"
#include "rebel/chrome/browser/mac/sparkle_glue.h"
#endif

void GetInstalledVersion(InstalledVersionCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
#if BUILDFLAG(REBEL_BROWSER)
      base::BindOnce([] {
        return InstalledAndCriticalVersion(base::Version(
            base::UTF16ToASCII(rebel::CurrentlyDownloadedVersion())));
      }),
#else
      base::BindOnce([] {
        return InstalledAndCriticalVersion(
            base::Version(CurrentlyInstalledVersion()));
      }),
#endif
      std::move(callback));
}
