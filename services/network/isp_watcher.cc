#include "services/network/isp_watcher.h"

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/isp/isp.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

constexpr base::TimeDelta kHttpRequestTimeout = base::Seconds(10);
constexpr base::TimeDelta kHttpRetryInterval = base::Seconds(10);
constexpr base::TimeDelta kISPWatchInterval = base::Minutes(1);

// We get our public IP address from this service
const char kIPIFYAPIURL[] = "https://api.ipify.org/";

// We get our current ISP from this service
const char kIPAPIISPURL[] = "http://ip-api.com/line/?fields=status,isp";

const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

void PostTask(base::OnceClosure task) {
  GetTaskRunner()->PostTask(FROM_HERE, std::move(task));
}

void PostDelayedTask(base::OnceClosure task, base::TimeDelta delay) {
  GetTaskRunner()->PostDelayedTask(FROM_HERE, std::move(task),
                                   std::move(delay));
}

net::URLRequestContext* GetURLRequestContext() {
  static base::NoDestructor<std::unique_ptr<net::URLRequestContext>>
      url_request_context(nullptr);

  if (!*url_request_context) {
    net::URLRequestContextBuilder builder;
    builder.DisableHttpCache();
    builder.set_proxy_config_service(
        std::make_unique<net::ProxyConfigServiceFixed>(
            net::ProxyConfigWithAnnotation::CreateDirect()));
    *url_request_context = builder.Build();
  }
  return url_request_context->get();
}

struct ContextGetter : net::URLRequestContextGetter {
  net::URLRequestContext* GetURLRequestContext() override {
    return ::GetURLRequestContext();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return ::GetTaskRunner();
  }

 private:
  ~ContextGetter() override {}
};

static base::NoDestructor<
    absl::optional<std::function<network::mojom::URLLoaderFactory*()>>>
    url_loader_factory_provider_;

network::mojom::URLLoaderFactory* GetURLLoaderFactory() {
  if (*url_loader_factory_provider_) {
    return (**url_loader_factory_provider_)();
  }
  static base::NoDestructor<
      std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>>
      transitional_url_loader_factory_owner(nullptr);

  if (!*transitional_url_loader_factory_owner) {
    *transitional_url_loader_factory_owner =
        std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
            base::MakeRefCounted<ContextGetter>());
  }

  return (*transitional_url_loader_factory_owner)->GetURLLoaderFactory().get();
}

std::unique_ptr<network::SimpleURLLoader> MakeLoader(
    const GURL& url,
    const std::string& method,
    std::map<std::string, std::string>&& headers,
    base::TimeDelta timeout_duration) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = method;

  for (const auto& [key, value] : headers) {
    resource_request->headers.SetHeader(key, value);
  }

  static net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("isp_request",
                                          R"(
            semantics {
              sender: "ISP Request"
              description:
                "Find the current public IP and ISP."
              trigger: "Periodic check once per minute."
              data: "None."
              destination: WEBSITE
            }
            policy {
              cookies_allowed: NO
              policy_exception_justification: "Not implemented."
              setting: "This feature can be disabled in chrome flags."
            })");

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_loader->SetTimeoutDuration(timeout_duration);

  return simple_loader;
}

using RequestCompletionOnceCallback =
    base::OnceCallback<void(int response_code, std::string_view response_body)>;

class HTTPRequest : public std::enable_shared_from_this<HTTPRequest> {
 public:
  HTTPRequest(GURL url,
              const std::string& method,
              RequestCompletionOnceCallback callback)
      : url_(url), callback_(std::move(callback)) {
    loader_ = MakeLoader(url, method, {}, kHttpRequestTimeout);

    loader_->SetAllowHttpErrorResults(false);
  }

  void Start() {
    constexpr size_t kMaxResponseSizeDefault = 1024;

    loader_->SetOnResponseStartedCallback(base::BindOnce(
        [](std::shared_ptr<HTTPRequest> _this, const GURL& _final_url,
           const network::mojom::URLResponseHead& response_head) {
          _this->OnResponseStarted(_final_url, response_head);
        },
        shared_from_this()));

    VLOG(2) << "Started an HTTP request to " << url_;

    loader_->DownloadToString(
        GetURLLoaderFactory(),
        base::BindOnce(
            [](std::shared_ptr<HTTPRequest> _this,
               std::unique_ptr<std::string> response_body) {
              _this->OnBodyAsString(std::move(response_body));
            },
            shared_from_this()),
        kMaxResponseSizeDefault);
  }

  void OnResponseStarted(const GURL& _final_url,
                         const network::mojom::URLResponseHead& response_head) {
    response_code_ = response_head.headers->response_code();
  }

  void OnBodyAsString(std::unique_ptr<std::string> response_body_ptr) {
    std::string_view response_body;
    if (response_body_ptr) {
      response_body = {response_body_ptr->data(), response_body_ptr->size()};
    }
    VLOG(2) << "Got an HTTP response from " << url_ << " with response code "
            << response_code_ << ", and body " << response_body;
    std::move(callback_).Run(response_code_, response_body);
    loader_ = nullptr;
  }

 private:
  std::unique_ptr<network::SimpleURLLoader> loader_;
  GURL url_;
  int response_code_;
  RequestCompletionOnceCallback callback_;
};

using ResponseBodyCallback =
    base::OnceCallback<void(std::unique_ptr<std::string> response_body)>;

void HTTPGet(GURL url, RequestCompletionOnceCallback callback) {
  std::shared_ptr<HTTPRequest> request =
      std::make_shared<HTTPRequest>(std::move(url), "GET", std::move(callback));
  request->Start();
}

std::string_view trim_whitespace(std::string_view str) {
  std::size_t start = str.find_first_not_of(" \t\n\r\f\v");
  std::size_t end = str.find_last_not_of(" \t\n\r\f\v");
  return str.substr(start, end - start + 1);
}

using IPCompletionOnceCallback =
    base::OnceCallback<void(bool ok, std::string ip)>;

void QueryIP(IPCompletionOnceCallback callback) {
  HTTPGet(GURL(kIPIFYAPIURL),
          base::BindOnce(
              [](IPCompletionOnceCallback callback, int status_code,
                 std::string_view response_body) {
                if (status_code != 200) {
                  std::move(callback).Run(false, {});
                  return;
                }
                std::string ip(trim_whitespace(response_body));
                std::move(callback).Run(true, ip);
              },
              std::move(callback)));
}

using ISPCompletionOnceCallback =
    base::OnceCallback<void(bool ok, std::string isp)>;

void QueryISP(IPCompletionOnceCallback callback) {
  HTTPGet(GURL(kIPAPIISPURL),
          base::BindOnce(
              [](IPCompletionOnceCallback callback, int status_code,
                 std::string_view response_body) {
                if (status_code == 200) {
                  std::size_t sep = response_body.find('\n');
                  std::string_view status =
                      trim_whitespace(response_body.substr(0, sep));
                  std::string_view isp =
                      trim_whitespace(response_body.substr(sep + 1));

                  if (status == "success") {
                    std::move(callback).Run(true, std::string(isp));
                    return;
                  }
                }
                std::move(callback).Run(false, {});
              },
              std::move(callback)));
}

void UpdateISP() {
  QueryISP(base::BindOnce([](bool ok, std::string isp) {
    if (ok) {
      VLOG(1) << "The ISP has changed to " << isp;
      net::ISP::SetISP(isp);
    } else {
      PostDelayedTask(base::BindOnce(UpdateISP), kHttpRetryInterval);
    }
  }));
}

void WatchIP() {
  QueryIP(base::BindOnce([](bool ok, std::string ip) {
    if (ok) {
      bool ip_changed = net::ISP::SetIP(ip);
      if (ip_changed) {
        VLOG(1) << "The public IP has changed to " << ip;
        PostTask(base::BindOnce(UpdateISP));
      }
    }
    PostDelayedTask(base::BindOnce(WatchIP), kISPWatchInterval);
  }));
}

static bool started = false;

// static
void ISPWatcher::Start() {
  if (started) {
    return;
  }
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          "enable-per-isp-histograms")) {
    return;
  }
  net::ISP::Initialize();
  VLOG(2) << "ISP watcher service started";
  PostTask(base::BindOnce(WatchIP));
  started = true;
}

// static
void ISPWatcher::StartWithURLLoaderFactoryProvider(
    std::function<network::mojom::URLLoaderFactory*()>
        url_loader_factory_provider) {
  if (started) {
    return;
  }
  url_loader_factory_provider_->emplace(url_loader_factory_provider);
  Start();
}
