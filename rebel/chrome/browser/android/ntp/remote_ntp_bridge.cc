// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/android/ntp/remote_ntp_bridge.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/RemoteNtpBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_impl.h"
#include "rebel/chrome/browser/ui/ntp/remote_ntp_tab_helper.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace rebel {

RemoteNtpBridge::RemoteNtpBridge(JNIEnv* env,
                                 const JavaRef<jobject>& obj,
                                 const JavaRef<jobject>& j_web_contents,
                                 jboolean j_dark_mode_enabled)
    : weak_java_ref_(env, obj), weak_ptr_factory_(this) {
  web_contents_ = content::WebContents::FromJavaWebContents(j_web_contents);

  rebel::RemoteNtpTabHelper* remote_ntp_tab_helper =
      rebel::RemoteNtpTabHelper::FromWebContents(web_contents_);
  if (remote_ntp_tab_helper) {
    remote_ntp_tab_helper->SetRemoteNtpBridge(this);
  }

  auto* remote_ntp_service = rebel::RemoteNtpServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (remote_ntp_service) {
    bool dark_mode_enabled = static_cast<bool>(j_dark_mode_enabled);
    remote_ntp_service->SetDarkModeEnabled(dark_mode_enabled);
  }
}

RemoteNtpBridge::~RemoteNtpBridge() = default;

void RemoteNtpBridge::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  rebel::RemoteNtpTabHelper* remote_ntp_tab_helper =
      rebel::RemoteNtpTabHelper::FromWebContents(web_contents_);

  if (remote_ntp_tab_helper) {
    remote_ntp_tab_helper->SetRemoteNtpBridge(nullptr);
  }

  delete this;
}

void RemoteNtpBridge::LoadInternalUrl(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  const auto jurl = base::android::ConvertUTF8ToJavaString(env, url.spec());
  Java_RemoteNtpBridge_loadInternalUrl(env, obj, jurl);
}

void RemoteNtpBridge::LoadAutocompleteMatchUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  const auto jurl = base::android::ConvertUTF8ToJavaString(env, url.spec());
  Java_RemoteNtpBridge_loadAutocompleteMatchUrl(env, obj, jurl,
                                                transition_type);
}

static jlong JNI_RemoteNtpBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents,
    jboolean j_dark_mode_enabled) {
  RemoteNtpBridge* bridge =
      new RemoteNtpBridge(env, obj, j_web_contents, j_dark_mode_enabled);
  return reinterpret_cast<intptr_t>(bridge);
}

static jboolean JNI_RemoteNtpBridge_IsRemoteNtpEnabled(JNIEnv* env) {
  bool is_remote_ntp_enabled = rebel::IsRemoteNtpEnabled();
  return static_cast<jboolean>(is_remote_ntp_enabled);
}

static jboolean JNI_RemoteNtpBridge_IsRemoteNtpUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jurl) {
  const GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));

  const bool is_remote_ntp_url = RemoteNtpService::IsRemoteNtpUrl(url);
  return static_cast<jboolean>(is_remote_ntp_url);
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_RemoteNtpBridge_GetRemoteNtpUrl(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(env, rebel::GetRemoteNtpUrl());
}

}  // namespace rebel
