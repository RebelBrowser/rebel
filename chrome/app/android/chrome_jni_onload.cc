// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_jni_onload.h"

#include "chrome/app/android/chrome_main_delegate_android.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"

#if BUILDFLAG(REBEL_BROWSER)
#include "rebel/chrome/app/rebel_main_delegate.h"
#endif

namespace android {

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

#if BUILDFLAG(REBEL_BROWSER)
  content::SetContentMainDelegate(new rebel::RebelMainDelegate());
#else
  content::SetContentMainDelegate(new ChromeMainDelegateAndroid());
#endif
  return true;
}

}  // namespace android
