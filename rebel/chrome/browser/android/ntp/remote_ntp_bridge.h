// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_ANDROID_NTP_REMOTE_NTP_BRIDGE_H_
#define REBEL_CHROME_BROWSER_ANDROID_NTP_REMOTE_NTP_BRIDGE_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/page_transition_types.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;

namespace rebel {

// The delegate to monitor the native RemoteNTP objects, to forward events from
// the RemoteNTP bridge.
class RemoteNtpBridge {
 public:
  RemoteNtpBridge(JNIEnv* env,
                  const base::android::JavaRef<jobject>& obj,
                  const base::android::JavaRef<jobject>& j_web_contents,
                  jboolean j_dark_mode_enabled);

  void Destroy(JNIEnv*, const base::android::JavaParamRef<jobject>&);

  void LoadInternalUrl(const GURL& url);
  void LoadAutocompleteMatchUrl(const GURL& url,
                                ui::PageTransition transition_type);

  void UpdateWiFiStatus();
  void OnWiFiStatusChanged(
      JNIEnv*,
      const base::android::JavaParamRef<jobject>&,
      const base::android::JavaParamRef<jobjectArray>& j_wifi_status);

 private:
  ~RemoteNtpBridge();

  raw_ptr<content::WebContents> web_contents_;
  JavaObjectWeakGlobalRef weak_java_ref_;

  base::WeakPtrFactory<RemoteNtpBridge> weak_ptr_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_ANDROID_NTP_REMOTE_NTP_BRIDGE_H_
