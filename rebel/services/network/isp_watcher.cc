// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/services/network/isp_watcher.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/url_loader_factory.h"

#include "rebel/net/isp/isp.h"

namespace rebel {

namespace {

constexpr base::TimeDelta kHttpRequestTimeout = base::Seconds(10);
constexpr base::TimeDelta kHttpRetryInterval = base::Seconds(10);
constexpr base::TimeDelta kISPWatchInterval = base::Minutes(1);

// We get our public IP address from this service.
const char kIPWatchURL[] = "https://api.ipify.org/";

// We get our current ISP from this service.
const char kISPWatchURL[] = "http://ip-api.com/line/?fields=status,isp";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("remote_ntp_api_allow_list", R"(
        semantics {
          sender: "ISP Request"
          description:
            "Find the current public IP and ISP."
          trigger: "Once per minute."
          destination: REBEL_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "Tjhis feature is disabled by default, and is only for testing."
        })");

}  // namespace

ISPWatcher::ISPWatcher(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      weak_ptr_factory_(this) {
  if (ISP::Instance()) {
    QueryIP();
  }
}

ISPWatcher::ISPWatcher(
    mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      weak_ptr_factory_(this) {
  if (ISP::Instance()) {
    QueryIP();
  }
}

ISPWatcher::~ISPWatcher() = default;

void ISPWatcher::QueryIP(absl::optional<base::TimeDelta> delay) {
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto callback = base::BindOnce(&ISPWatcher::PerformIPRequest,
                                 weak_ptr_factory_.GetWeakPtr());

  if (delay) {
    task_runner->PostDelayedTask(FROM_HERE, std::move(callback), *delay);
  } else {
    task_runner->PostTask(FROM_HERE, std::move(callback));
  }
}

void ISPWatcher::PerformIPRequest() {
  PerformGetRequest(GURL(kIPWatchURL),
                    base::BindOnce(&ISPWatcher::OnIPReceived,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ISPWatcher::OnIPReceived(std::unique_ptr<std::string> response) {
  if (!response) {
    QueryIP(kHttpRetryInterval);
    return;
  }

  std::string ip;
  base::TrimWhitespaceASCII(*response, base::TrimPositions::TRIM_ALL, &ip);

  if (ISP::Instance()->SetIP(std::move(ip))) {
    QueryISP();
    return;
  }

  QueryIP(kISPWatchInterval);
}

void ISPWatcher::QueryISP(absl::optional<base::TimeDelta> delay) {
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto callback = base::BindOnce(&ISPWatcher::PerformISPRequest,
                                 weak_ptr_factory_.GetWeakPtr());

  if (delay) {
    task_runner->PostDelayedTask(FROM_HERE, std::move(callback), *delay);
  } else {
    task_runner->PostTask(FROM_HERE, std::move(callback));
  }
}

void ISPWatcher::PerformISPRequest() {
  PerformGetRequest(GURL(kISPWatchURL),
                    base::BindOnce(&ISPWatcher::OnISPReceived,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ISPWatcher::OnISPReceived(std::unique_ptr<std::string> response) {
  if (!response) {
    QueryISP(kHttpRetryInterval);
    return;
  }

  auto status_and_isp = base::SplitString(
      *response, "\n", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  if (status_and_isp.size() != 2) {
    QueryISP(kHttpRetryInterval);
    return;
  }

  auto status = std::move(status_and_isp[0]);
  auto isp = std::move(status_and_isp[1]);

  if (status != "success") {
    QueryISP(kHttpRetryInterval);
    return;
  }

  ISP::Instance()->SetISP(std::move(isp));
  QueryIP(kISPWatchInterval);
}

void ISPWatcher::PerformGetRequest(const GURL& url,
                                   OnGetRequestComplete&& callback) {
  static constexpr size_t kMaxResponseSizeDefault = 1024;

  const auto origin = url::Origin::Create(url);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DISABLE_CACHE;
  resource_request->site_for_cookies = net::SiteForCookies::FromOrigin(origin);
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(origin);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotationTag);
  simple_url_loader_->SetTimeoutDuration(kHttpRequestTimeout);
  simple_url_loader_->SetAllowHttpErrorResults(false);

  simple_url_loader_->DownloadToString(GetURLLoader(), std::move(callback),
                                       kMaxResponseSizeDefault);
}

network::mojom::URLLoaderFactory* ISPWatcher::GetURLLoader() {
  return shared_url_loader_factory_
             ? static_cast<network::mojom::URLLoaderFactory*>(
                   shared_url_loader_factory_.get())
             : url_loader_factory_.get();
}

}  // namespace rebel
