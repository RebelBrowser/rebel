// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_SERVICES_NETWORK_ISP_WATCHER_H_
#define REBEL_SERVICES_NETWORK_ISP_WATCHER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace rebel {

class COMPONENT_EXPORT(NETWORK_CPP) ISPWatcher {
 public:
  // Construct an ISPWatcher from the browser process.
  explicit ISPWatcher(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  // Construct an ISPWatcher from the network service process.
  explicit ISPWatcher(
      mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory);

  ~ISPWatcher();

 private:
  void QueryIP(absl::optional<base::TimeDelta> delay = {});
  void PerformIPRequest();
  void OnIPReceived(std::unique_ptr<std::string> ip);

  void QueryISP(absl::optional<base::TimeDelta> delay = {});
  void PerformISPRequest();
  void OnISPReceived(std::unique_ptr<std::string> isp);

  using OnGetRequestComplete =
      base::OnceCallback<void(std::unique_ptr<std::string>)>;
  void PerformGetRequest(const GURL& url, OnGetRequestComplete&&);

  network::mojom::URLLoaderFactory* GetURLLoader();

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<ISPWatcher> weak_ptr_factory_;
};

}  // namespace rebel

#endif  // REBEL_SERVICES_NETWORK_ISP_WATCHER_H_
