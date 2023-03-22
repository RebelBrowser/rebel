// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/services/network/remote_ntp_modem_data.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace rebel {

namespace {

constexpr char kRemoteNtpModemDataUrl[] =
    "http://192.168.100.1/index.cgi?page=modemStatusData";

constexpr char kRemoteNtpModemDatCommandLine[] = "remote-ntp-modem-data-url";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("remote_ntp_modem_data", R"(
        semantics {
          sender: "Remote NTP Modem Data Fetcher"
          description:
            "Viasat Browser tries to fetch and parse the Viasat Internet modem "
            "index.cgi API to gather data such as the modem's MAC address and
            FL/RL metrics."
          trigger: "On demand from the rebel.network API."
          destination: VIASAT_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");

constexpr base::TimeDelta kTimeoutDuration = base::Seconds(5);
constexpr base::TimeDelta kFailedRetryBackoff = base::Seconds(3);
constexpr int kFailedRetryAttempts = 5;

constexpr RemoteNtpModemData::Configuration kConfiguration{
    kRemoteNtpModemDataUrl,
    kRemoteNtpModemDatCommandLine,
    RemoteNtpModemData::ResponseType::Raw,
    net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE,
    kTrafficAnnotationTag,
    base::TimeDelta(),
    kTimeoutDuration,
    kFailedRetryBackoff,
    kFailedRetryAttempts,
    true};

// See section 3.5 Modem Status Data, table 7:
// https://share.viasat.com/leapfrog/Technical/Software/_layouts/15/WopiFrame.aspx?sourcedoc=/leapfrog/Technical/Software/Documents/UT/Design/Web%20GUI%20ICD.docx&action=default
constexpr std::size_t kMacAddress = 1;
constexpr std::size_t kModemVersion = 2;
constexpr std::size_t kUtStatus = 4;
constexpr std::size_t kOnlineTime = 9;
constexpr std::size_t kAccelerationStatus = 26;
constexpr std::size_t kFlSnr = 11;
constexpr std::size_t kFlPower = 14;
constexpr std::size_t kModemType = 47;
constexpr std::size_t kModemTemperature = 64;
constexpr std::size_t kRlPower = 65;
constexpr std::size_t kRlSymbolRate = 53;
constexpr std::size_t kRlModCode = 54;
constexpr std::size_t kLEDStatus = 71;
constexpr std::size_t kLEDColor = 72;
constexpr std::size_t kIDUTemperature = 79;

}  // namespace

RemoteNtpModemData::RemoteNtpModemData(
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : DataFetcher(kConfiguration,
                  nullptr,
                  std::move(shared_url_loader_factory)),
      delegate_(delegate) {}

RemoteNtpModemData::~RemoteNtpModemData() = default;

bool RemoteNtpModemData::OnFetchComplete(const std::string& data,
                                         std::string* error_message) {
  auto lines = base::SplitStringPieceUsingSubstr(
      data, "##", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_ALL);

  auto get_string_at_index = [&lines](std::size_t index) {
    if (lines.size() <= index) {
      return std::string();
    }
    return std::string(lines[index]);
  };

  auto get_number_at_index = [&lines](std::size_t index) {
    double result = 0.0;

    if (lines.size() <= index) {
      return result;
    }

    const auto& field = lines[index];
    if (!base::StringToDouble(field, &result)) {
      result = 0.0;
    }

    return result;
  };

  ModemData modem_data;
  modem_data.mac_address = get_string_at_index(kMacAddress);
  modem_data.modem_version = get_string_at_index(kModemVersion);
  modem_data.modem_type = get_string_at_index(kModemType);
  modem_data.ut_status = get_string_at_index(kUtStatus);
  modem_data.acceleration_status = get_string_at_index(kAccelerationStatus);
  modem_data.online_time = get_string_at_index(kOnlineTime);
  modem_data.led_status = get_string_at_index(kLEDStatus);
  modem_data.led_color = get_string_at_index(kLEDColor);
  modem_data.modem_temperature = get_number_at_index(kModemTemperature);
  modem_data.idu_temperature = get_number_at_index(kIDUTemperature);
  modem_data.fl_snr = get_number_at_index(kFlSnr);
  modem_data.fl_power = get_number_at_index(kFlPower);
  modem_data.rl_power = get_number_at_index(kRlPower);
  modem_data.rl_symbol_rate = get_number_at_index(kRlSymbolRate);
  modem_data.rl_mod_code = get_string_at_index(kRlModCode);

  delegate_->OnModemDataChanged(std::move(modem_data));
  return true;
}

RemoteNtpModemData::ModemData::ModemData() = default;
RemoteNtpModemData::ModemData::~ModemData() = default;

RemoteNtpModemData::ModemData::ModemData(RemoteNtpModemData::ModemData&&) =
    default;
RemoteNtpModemData::ModemData& RemoteNtpModemData::ModemData::operator=(
    RemoteNtpModemData::ModemData&&) = default;

}  // namespace rebel
