// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_SERVICES_NETWORK_DATA_FETCHER_H_
#define REBEL_SERVICES_NETWORK_DATA_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/branding_buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

#include "rebel/build/buildflag.h"

namespace base {
class ListValue;
class Value;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class PrefService;

// Define constexpr strings for the preferences used by |DataFetcher|.
#define DATA_FETCHER_PREF_NAMES(identifier, version)                           \
  static constexpr const char* kIdentifier =                                   \
      REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME_PATH) "." identifier;          \
  static constexpr const char* kUrlPrefName =                                  \
      REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME_PATH) "." identifier           \
                                                      ".url.version_" version; \
  static constexpr const char* kLastFetchPrefName = REBEL_STRING_BUILDFLAG(    \
      REBEL_BROWSER_NAME_PATH) "." identifier ".last_fetch.version_" version;  \
  static constexpr const char* kCachedFetchPrefName = REBEL_STRING_BUILDFLAG(  \
      REBEL_BROWSER_NAME_PATH) "." identifier                                  \
                               ".cached_fetch.version_" version;

namespace rebel {

// Utility class to fetch a remote URL that is expected to respond with a small
// JSON structure or any raw data file. Caches the fetched response in the pref
// service.
//
// Implementations must use |DATA_FETCHER_PREF_NAMES| to generate strings which
// are used to generate preference names used for the cache. New implementations
// should also: add themselves to one of the pref registration methods inside
// //rebel/chrome/browser/prefs/browser_prefs.cc and to the list of explicit
// instantiations at the bottom of data_fetcher.cc.
template <typename T>
class COMPONENT_EXPORT(NETWORK_CPP) DataFetcher {
 public:
  enum class ResponseType {
    JSON,
    Raw,
  };

  struct Configuration {
    // URL to fetch and command line switch that can override the URL.
    const char* default_url_{nullptr};
    const char* override_url_switch_{nullptr};

    // Fetch options.
    const ResponseType response_type_;
    const int extra_load_flags_{0};

    // Network traffic tag used when fetching the remote URL.
    const net::NetworkTrafficAnnotationTag traffic_annotation_tag_;

    // Options for when the remote URL should be refetched. Set to 0 to disable.
    const base::TimeDelta refetch_interval_;
    const base::TimeDelta timeout_duration_;
    const base::TimeDelta failed_retry_backoff_;
    const int failed_retry_attempts_{0};

    // Whether to call OnFetchComplete with empty dict on failure.
    const bool notify_on_failure_{false};
  };

  // Constructor for data fetchers that live in the browser process.
  DataFetcher(
      Configuration configuration,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  // Constructor for data fetchers that live in the network process.
  DataFetcher(
      Configuration configuration,
      PrefService* prefs,
      mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory);

  virtual ~DataFetcher();

  // Check if the remote URL should be fetched and, if so, trigger that fetch.
  // If configured to do so, schedules a task to call itself again in the future
  // to fetch a refreshed response.
  void MaybeStartFetch(bool force_fetch);

 protected:
  // Check if a response has been cached, and if so, trigger |OnFetchComplete|.
  void InitFromCache();

  // Called when the request to fetch the remote URL has completed and the
  // fetched JSON has been parsed successfully. The implementation may return a
  // boolean indicating whether or not it accepted the response.
  virtual bool OnFetchComplete(const base::Value& json,
                               std::string* error_message) {
    return true;
  }

  // Called when the request to fetch the remote URL has completed and the
  // configured response type was ResponseType::Raw. The implementation may
  // return a boolean indicating whether or not it accepted the response.
  virtual bool OnFetchComplete(const std::string& data,
                               std::string* error_message) {
    return true;
  }

  SEQUENCE_CHECKER(sequence_checker_);

  const Configuration configuration_;
  raw_ptr<PrefService> prefs_;

 private:
  DataFetcher(const DataFetcher&) = delete;
  DataFetcher& operator=(const DataFetcher&) = delete;

  // Get the remote URL to be fetched.
  GURL GetURLToFetch() const;

  // Fetch the remote URL, overwriting any cache in prefs that already exist.
  void StartFetch();

  // Called when the request to fetch the remote URL has completed. Triggers a
  // JSON parser on the response if the fetch was successful.
  void OnRequestComplete(std::unique_ptr<std::string> response_body);

  // Called when the JSON parser successfully parses the fetched response.
  // Notifies the implementation of the response, and if the implementation
  // accepts it, caches the response in the pref service.
  void OnJsonParsed(base::Value json);

  // Called when any network or JSON parser error occurs. If configured to do
  // so, schedules a task to retry the fetch in the future.
  void OnFailure(const std::string& error_message);

  // Post a task to fetch the remote URL in the future.
  void DelayedFetch(const base::TimeDelta delay);

  base::OneShotTimer fetch_timer_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  GURL pending_url_;
  int fetch_count_{0};

  base::WeakPtrFactory<DataFetcher> weak_ptr_factory_;
};

}  // namespace rebel

#endif  // REBEL_SERVICES_NETWORK_DATA_FETCHER_H_
