// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_SERVICES_NETWORK_REMOTE_NTP_MODEM_DATA_H_
#define REBEL_SERVICES_NETWORK_REMOTE_NTP_MODEM_DATA_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"

#include "rebel/services/network/data_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace rebel {

class COMPONENT_EXPORT(NETWORK_CPP) RemoteNtpModemData
    : public DataFetcher<RemoteNtpModemData> {
 public:
  struct ModemData {
    ModemData();
    ~ModemData();

    ModemData(const ModemData&) = delete;
    ModemData& operator=(const ModemData&) = delete;

    ModemData(ModemData&&);
    ModemData& operator=(ModemData&&);

    std::string mac_address;
    std::string modem_version;
    std::string modem_type;
    std::string ut_status;
    std::string acceleration_status;
    std::string online_time;
    std::string led_status;
    std::string led_color;
    double modem_temperature{0.0};
    double idu_temperature{0.0};
    double fl_snr{0.0};
    double fl_power{0.0};
    double rl_power{0.0};
    double rl_symbol_rate{0.0};
    std::string rl_mod_code;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when an icon has been successfully stored or updated.
    virtual void OnModemDataChanged(ModemData data) = 0;
  };

  RemoteNtpModemData(
      Delegate* delegate,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  ~RemoteNtpModemData() override;

  // This class doesn't use preferences, but they still must be declared.
  DATA_FETCHER_PREF_NAMES("remote_ntp_modem_data", "1")

 protected:
  // Overridden from rebel::DataFetcher:
  bool OnFetchComplete(const std::string& data,
                       std::string* error_message) override;

 private:
  RemoteNtpModemData(const RemoteNtpModemData&) = delete;
  RemoteNtpModemData& operator=(const RemoteNtpModemData&) = delete;

  Delegate* delegate_{nullptr};
};

}  // namespace rebel

#endif  // REBEL_SERVICES_NETWORK_REMOTE_NTP_MODEM_DATA_H_
