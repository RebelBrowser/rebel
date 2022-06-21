// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/app/rebel_main_delegate.h"

namespace rebel {

RebelMainDelegate::RebelMainDelegate() = default;
RebelMainDelegate::~RebelMainDelegate() = default;

#if !BUILDFLAG(IS_ANDROID)
RebelMainDelegate::RebelMainDelegate(base::TimeTicks exe_entry_point_ticks)
    : RebelMainDelegateBase(exe_entry_point_ticks) {}
#endif

absl::optional<int> RebelMainDelegate::PreBrowserMain() {
  return RebelMainDelegateBase::PreBrowserMain();
}

content::ContentClient* RebelMainDelegate::CreateContentClient() {
  return &rebel_content_client_;
}

}  // namespace rebel
