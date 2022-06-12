// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_SERVICES_NETWORK_REMOTE_NTP_API_ALLOW_LIST_H_
#define REBEL_SERVICES_NETWORK_REMOTE_NTP_API_ALLOW_LIST_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

#include "rebel/services/network/data_fetcher.h"

namespace base {
class Value;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class PrefRegistrySimple;
class PrefService;

namespace rebel {

class COMPONENT_EXPORT(NETWORK_CPP) RemoteNtpApiAllowList
    : public DataFetcher<RemoteNtpApiAllowList> {
  using AllowListType = std::vector<std::string>;

 public:
  RemoteNtpApiAllowList(
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  ~RemoteNtpApiAllowList() override;

  // Register preferences used by this class.
  static void RegisterProfilePrefs(PrefRegistrySimple* prefs);
  DATA_FETCHER_PREF_NAMES("remote_ntp_api_allow_list", "1")

  // Check if the allow list matches the given host.
  bool ContainsHost(const std::string& host) const;

 protected:
  bool OnFetchComplete(const base::Value& json,
                       std::string* error_message) override;

 private:
  RemoteNtpApiAllowList(const RemoteNtpApiAllowList&) = delete;
  RemoteNtpApiAllowList& operator=(const RemoteNtpApiAllowList&) = delete;

  // Transform the result of the JSON parser to a AllowListType, dropping
  // invalid values.
  AllowListType ParseAllowList(const base::Value& list) const;

  AllowListType parsed_allow_list_;
};

}  // namespace rebel

#endif  // REBEL_SERVICES_NETWORK_REMOTE_NTP_API_ALLOW_LIST_H_
