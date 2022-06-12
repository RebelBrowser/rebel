// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/services/network/remote_ntp_api_allow_list.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace rebel {

namespace {

constexpr char kRemoteNtpApiAllowListUrl[] =
    BUILDFLAG(REBEL_BROWSER_API_ALLOW_LIST);

constexpr char kRemoteNtpApiAllowListCommandLine[] =
    "remote-ntp-allow-list-url";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("remote_ntp_api_allow_list", R"(
        semantics {
          sender: "Remote NTP API Allow List Fetcher"
          description:
            "Rebel allows only a small set of domains to use RemoteNTP APIs. "
            "This service fetches the list of these sites."
          trigger: "Once per day."
          destination: REBEL_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");

constexpr base::TimeDelta kRedownloadInterval = base::Hours(24);
constexpr base::TimeDelta kFailedRetryBackoff = base::Minutes(1);
constexpr int kFailedRetryAttempts = std::numeric_limits<int>::max();

constexpr RemoteNtpApiAllowList::Configuration kConfiguration{
    kRemoteNtpApiAllowListUrl,
    kRemoteNtpApiAllowListCommandLine,
    RemoteNtpApiAllowList::ResponseType::JSON,
    net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE,
    kTrafficAnnotationTag,
    kRedownloadInterval,
    base::TimeDelta(),
    kFailedRetryBackoff,
    kFailedRetryAttempts,
    false};

}  // namespace

RemoteNtpApiAllowList::RemoteNtpApiAllowList(
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : DataFetcher(kConfiguration, prefs, std::move(shared_url_loader_factory)) {
  InitFromCache();
}

RemoteNtpApiAllowList::~RemoteNtpApiAllowList() = default;

void RemoteNtpApiAllowList::RegisterProfilePrefs(PrefRegistrySimple* prefs) {
  prefs->RegisterStringPref(kUrlPrefName, std::string());
  prefs->RegisterInt64Pref(kLastFetchPrefName, 0);
  prefs->RegisterListPref(kCachedFetchPrefName);
}

bool RemoteNtpApiAllowList::ContainsHost(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const AllowListType::value_type& pattern : parsed_allow_list_) {
    if (host.find(pattern) != std::string::npos) {
      return true;
    }
  }

  return false;
}

bool RemoteNtpApiAllowList::OnFetchComplete(const base::Value& json,
                                            std::string* error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!json.is_list()) {
    *error_message = "JSON is not a list";
    return false;
  }

  parsed_allow_list_ = ParseAllowList(json);
  return true;
}

RemoteNtpApiAllowList::AllowListType RemoteNtpApiAllowList::ParseAllowList(
    const base::Value& list) const {
  const auto& list_values = list.GetList();
  AllowListType parsed;

  for (size_t i = 0; i < list_values.size(); ++i) {
    const base::Value& value = list_values[i];
    if (!value.is_string()) {
      continue;
    }

    std::string value_str = value.GetString();
    if (value_str.empty()) {
      continue;
    }

    parsed.push_back(std::move(value_str));
  }

  return parsed;
}

}  // namespace rebel
