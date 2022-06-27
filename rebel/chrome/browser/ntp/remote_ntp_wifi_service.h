// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_WIFI_SERVICE_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_WIFI_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/wifi/wifi_service.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"
#include "rebel/chrome/common/ntp/remote_ntp_types.h"

namespace rebel {

using WiFiStatusChangedCallback =
    base::RepeatingCallback<void(rebel::RemoteNtpWiFiStatusList)>;

// Wrapper around the platform-specific wifi::WiFiService implementations to
// provide a generic interface for the RemoteNTP.
class RemoteNtpWifiService {
 public:
  RemoteNtpWifiService(std::unique_ptr<wifi::WiFiService> wifi_service,
                       WiFiStatusChangedCallback wifi_status_changed);
  ~RemoteNtpWifiService();

  void Shutdown();
  void Update();

 private:
  RemoteNtpWifiService(const RemoteNtpWifiService&) = delete;
  RemoteNtpWifiService& operator=(const RemoteNtpWifiService&) = delete;

  void OnReceivedNetworkGUIDs(
      const wifi::WiFiService::NetworkGuidList& network_guid_list);

  static rebel::RemoteNtpWiFiStatusList RequestNetworkProperties(
      wifi::WiFiService* wifi_service,
      std::vector<std::string> network_guid_list);

  void OnReceivedWiFiStatus(rebel::RemoteNtpWiFiStatusList wifi_status);

  // Ensure that all calls to WiFiService are called from the same task runner
  // since the implementations do not provide any thread safety gaurantees.
  scoped_refptr<base::SequencedTaskRunner> wifi_service_task_runner_;

  std::unique_ptr<wifi::WiFiService> wifi_service_;
  WiFiStatusChangedCallback wifi_status_changed_;

  base::WeakPtrFactory<RemoteNtpWifiService> weak_factory_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_WIFI_SERVICE_H_
