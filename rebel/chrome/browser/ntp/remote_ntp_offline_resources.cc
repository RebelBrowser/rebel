// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_offline_resources.h"

#include "build/build_config.h"
#include "rebel/grit/rebel_resources.h"

// Note: This header file is generated during runhooks. It contains the
// definition of |detail::kRemoteNtpOfflineResources|.
#include "rebel/third_party/remote_ntp/remote_ntp_offline_resources.h"

namespace rebel {

base::span<const RemoteNtpOfflineResource> kRemoteNtpOfflineResources =
    detail::kRemoteNtpOfflineResources;

}
