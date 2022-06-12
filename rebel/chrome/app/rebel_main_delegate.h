// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_APP_REBEL_MAIN_DELEGATE_H_
#define REBEL_CHROME_APP_REBEL_MAIN_DELEGATE_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/app/android/chrome_main_delegate_android.h"
#else
#include "chrome/app/chrome_main_delegate.h"
#endif

#include "rebel/chrome/common/rebel_content_client.h"

namespace content {
class ContentBrowserClient;
class ContentClient;
class ContentRendererClient;
}  // namespace content

namespace rebel {

#if BUILDFLAG(IS_ANDROID)
using RebelMainDelegateBase = ChromeMainDelegateAndroid;
#else
using RebelMainDelegateBase = ChromeMainDelegate;
#endif

// Rebel implementation of ContentMainDelegate.
class RebelMainDelegate : public RebelMainDelegateBase {
 public:
  RebelMainDelegate();
  ~RebelMainDelegate() override;

#if !BUILDFLAG(IS_ANDROID)
  // ChromeMainDelegateAndroid does not override this constructor.
  explicit RebelMainDelegate(base::TimeTicks exe_entry_point_ticks);
#endif

 protected:
  RebelMainDelegate(const RebelMainDelegate&) = delete;
  RebelMainDelegate& operator=(const RebelMainDelegate&) = delete;

  // content::ContentMainDelegate:
  absl::optional<int> PreBrowserMain() override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  RebelContentClient rebel_content_client_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_APP_REBEL_MAIN_DELEGATE_H_
