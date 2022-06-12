// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_wifi_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/wifi/network_properties.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

namespace rebel {

namespace {

// Deletes WiFiService object on the worker thread.
void ShutdownWifiServiceOnWorkerThread(
    std::unique_ptr<wifi::WiFiService> wifi_service) {
  DCHECK(wifi_service.get());
}

}  // namespace

RemoteNtpWifiService::RemoteNtpWifiService(
    std::unique_ptr<wifi::WiFiService> wifi_service,
    WiFiStatusChangedCallback wifi_status_changed)
    : wifi_service_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}))),
      wifi_service_(std::move(wifi_service)),
      wifi_status_changed_(std::move(wifi_status_changed)),
      weak_factory_(this) {
  wifi_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&wifi::WiFiService::Initialize,
                                base::Unretained(wifi_service_.get()),
                                wifi_service_task_runner_));

  wifi_service_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &wifi::WiFiService::SetEventObservers,
          base::Unretained(wifi_service_.get()),
          base::SingleThreadTaskRunner::GetCurrentDefault(), base::DoNothing(),
          base::BindRepeating(&RemoteNtpWifiService::OnReceivedNetworkGUIDs,
                              weak_factory_.GetWeakPtr())));
}

RemoteNtpWifiService::~RemoteNtpWifiService() {
  // Verify that wifi_service_ was passed to ShutdownWifiServiceOnWorkerThread
  // to be deleted after completion of all posted tasks.
  DCHECK(!wifi_service_.get());
}

void RemoteNtpWifiService::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  wifi_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ShutdownWifiServiceOnWorkerThread,
                                std::move(wifi_service_)));
}

void RemoteNtpWifiService::Update() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  wifi_service_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&wifi::WiFiService::RequestNetworkScan,
                                base::Unretained(wifi_service_.get())));
}

void RemoteNtpWifiService::OnReceivedNetworkGUIDs(
    const wifi::WiFiService::NetworkGuidList& network_guid_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  wifi_service_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RequestNetworkProperties,
                     base::Unretained(wifi_service_.get()), network_guid_list),
      base::BindOnce(&RemoteNtpWifiService::OnReceivedWiFiStatus,
                     weak_factory_.GetWeakPtr()));
}

// This must be static in order to comply with |PostTaskAndReplyWithResult|,
// which forbids binding the first task to a weak pointer.
rebel::RemoteNtpWiFiStatusList RemoteNtpWifiService::RequestNetworkProperties(
    wifi::WiFiService* wifi_service,
    std::vector<std::string> network_guid_list) {
  rebel::RemoteNtpWiFiStatusList wifi_status;

  for (const auto& network_guid : network_guid_list) {
    wifi::NetworkProperties properties;
    std::string error;

    wifi_service->GetNetworkProperties(network_guid, &properties, &error);
    if (!error.empty()) {
      continue;
    }

    auto status = rebel::mojom::WiFiStatus::New();
    status->ssid = properties.ssid;
    status->bssid = properties.bssid;
    status->connection_state = properties.connection_state;
    status->rssi = properties.signal_strength;
    status->frequency = static_cast<int>(properties.frequency);
    status->link_speed = properties.link_speed;
    status->rx_mbps = properties.rx_mbps;
    status->tx_mbps = properties.tx_mbps;
    status->noise_measurement = properties.noise_measurement;

    wifi_status.push_back(std::move(status));
  }

  return wifi_status;
}

void RemoteNtpWifiService::OnReceivedWiFiStatus(
    rebel::RemoteNtpWiFiStatusList wifi_status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (wifi_status.empty()) {
    return;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows, |rssi| originates from the WLAN_AVAILABLE_NETWORK structure's
  // |wlanSignalQuality| field. This field is a percentage from 0% - 100%, so we
  // need to interpolate the percentage to derive RSSI, where 0% corresponds to
  // an RSSI of -100 dBm, and 100% corresponds to -50 dBm.
  // https://docs.microsoft.com/en-us/windows/win32/api/wlanapi/ns-wlanapi-wlan_available_network
  for (auto& wifi : wifi_status) {
    if (wifi->rssi >= 0) {
      wifi->rssi = (wifi->rssi / 2) - 100;
    }
  }
#endif

  wifi_status_changed_.Run(std::move(wifi_status));
}

}  // namespace rebel
