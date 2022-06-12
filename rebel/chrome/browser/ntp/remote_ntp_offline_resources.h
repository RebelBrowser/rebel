// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_OFFLINE_RESOURCES_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_OFFLINE_RESOURCES_H_

#include <cstdlib>

namespace rebel {

// Meta-info about resources declared in remote_ntp_resources.grdp.
struct RemoteNtpOfflineResource {
  int identifier;
  const char* file_path;
  const char* mime_type;
};

extern const RemoteNtpOfflineResource kRemoteNtpOfflineResources[];
extern const size_t kRemoteNtpOfflineResourcesSize;

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_OFFLINE_RESOURCES_H_
