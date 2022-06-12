// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/services/network/data_fetcher.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#include "rebel/services/network/remote_ntp_api_allow_list.h"

namespace rebel {

template <typename T>
DataFetcher<T>::DataFetcher(
    Configuration configuration,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : configuration_(configuration),
      prefs_(prefs),
      shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

template <typename T>
DataFetcher<T>::DataFetcher(
    Configuration configuration,
    PrefService* prefs,
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory)
    : configuration_(configuration),
      prefs_(prefs),
      url_loader_factory_(std::move(url_loader_factory)),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

template <typename T>
DataFetcher<T>::~DataFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetch_timer_.AbandonAndStop();
}

template <typename T>
void DataFetcher<T>::InitFromCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* cached =
      prefs_ ? prefs_->GetUserPrefValue(T::kCachedFetchPrefName) : nullptr;

  if (cached != nullptr) {
    std::string error_message;
    bool result = OnFetchComplete(*cached, &error_message);
    DCHECK(result) << error_message;
  }
}

template <typename T>
void DataFetcher<T>::MaybeStartFetch(bool force_fetch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Time now = base::Time::Now();
  const base::Time last_fetch_time =
      prefs_ ? base::Time::FromInternalValue(
                   prefs_->GetInt64(T::kLastFetchPrefName))
             : base::Time();
  const base::TimeDelta time_since_last_fetch = now - last_fetch_time;

  pending_url_ = GetURLToFetch();
  if (!pending_url_.is_valid()) {
    return;
  }

  // If configured to refetch, do so if enough time has passed, if the URL has
  // changed, or if the clock has gone backwards.
  bool refetch = false;

  if (!configuration_.refetch_interval_.is_zero()) {
    refetch = !prefs_ ||
              (time_since_last_fetch > configuration_.refetch_interval_) ||
              (pending_url_.spec() != prefs_->GetString(T::kUrlPrefName)) ||
              (now < last_fetch_time);
  }

  DelayedFetch(configuration_.refetch_interval_);

  if (force_fetch || refetch || last_fetch_time.is_null()) {
    StartFetch();
  }
}

template <typename T>
GURL DataFetcher<T>::GetURLToFetch() const {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  GURL override_url(
      command_line->GetSwitchValueASCII(configuration_.override_url_switch_));

  if (override_url.is_valid()) {
    return override_url;
  }

  return GURL(configuration_.default_url_);
}

template <typename T>
void DataFetcher<T>::StartFetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++fetch_count_;

  const auto origin = url::Origin::Create(pending_url_);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = pending_url_;
  resource_request->method = "GET";
  resource_request->load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | configuration_.extra_load_flags_;
  resource_request->site_for_cookies = net::SiteForCookies::FromOrigin(origin);
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(origin);

  auto* url_loader_factory =
      shared_url_loader_factory_
          ? static_cast<network::mojom::URLLoaderFactory*>(
                shared_url_loader_factory_.get())
          : url_loader_factory_.get();

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), configuration_.traffic_annotation_tag_);
  simple_url_loader_->SetRetryOptions(
      1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  if (!configuration_.timeout_duration_.is_zero()) {
    simple_url_loader_->SetTimeoutDuration(configuration_.timeout_duration_);
  }

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, base::BindOnce(&DataFetcher<T>::OnRequestComplete,
                                         weak_ptr_factory_.GetWeakPtr()));
}

template <typename T>
void DataFetcher<T>::OnRequestComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = simple_url_loader_->NetError();
  simple_url_loader_.reset();

  if (!response_body) {
    const std::string error_message =
        base::StringPrintf("No response received (net_error=%d)", net_error);
    OnFailure(error_message);
    return;
  }

  switch (configuration_.response_type_) {
    case ResponseType::JSON: {
      base::JSONReader::Result result =
          base::JSONReader::ReadAndReturnValueWithError(
              *response_body, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                  base::JSON_ALLOW_TRAILING_COMMAS);

      if (result.has_value()) {
        OnJsonParsed(std::move(*result));
      } else {
        OnFailure(result.error().message);
      }

      break;
    }

    case ResponseType::Raw: {
      std::string error_message;

      if (OnFetchComplete(*response_body, &error_message)) {
        if (prefs_) {
          prefs_->SetString(T::kUrlPrefName, pending_url_.spec());
          prefs_->SetInt64(T::kLastFetchPrefName,
                           base::Time::Now().ToInternalValue());
          prefs_->SetString(T::kCachedFetchPrefName, *response_body);
        }
      } else {
        OnFailure(error_message);
      }

      break;
    }
  }
}

template <typename T>
void DataFetcher<T>::OnJsonParsed(base::Value json) {
  std::string error_message;

  if (OnFetchComplete(json, &error_message)) {
    if (prefs_) {
      prefs_->SetString(T::kUrlPrefName, pending_url_.spec());
      prefs_->SetInt64(T::kLastFetchPrefName,
                       base::Time::Now().ToInternalValue());
      prefs_->Set(T::kCachedFetchPrefName, json);
    }
  } else {
    OnFailure(error_message);
  }
}

template <typename T>
void DataFetcher<T>::OnFailure(const std::string& error_message) {
  if (configuration_.notify_on_failure_) {
    std::string error;

    switch (configuration_.response_type_) {
      case ResponseType::JSON:
        OnFetchComplete(base::Value(base::Value::Type::DICT), &error);
        break;
      case ResponseType::Raw:
        OnFetchComplete(std::string{}, &error);
        break;
    }
  }

  if (fetch_count_ < configuration_.failed_retry_attempts_) {
    DelayedFetch(configuration_.failed_retry_backoff_);
  }
}

template <typename T>
void DataFetcher<T>::DelayedFetch(const base::TimeDelta delay) {
  if (!delay.is_zero()) {
    fetch_timer_.Start(FROM_HERE, delay,
                       base::BindOnce(&DataFetcher<T>::MaybeStartFetch,
                                      weak_ptr_factory_.GetWeakPtr(), false));
  }
}

// Explicitly declare the implementations of |DataFetcher|.
template class DataFetcher<RemoteNtpApiAllowList>;

}  // namespace rebel
