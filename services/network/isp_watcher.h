#ifndef SERVICES_NETWORK_ISP_WATCHER_H_
#define SERVICES_NETWORK_ISP_WATCHER_H_

#include <functional>

#include "services/network/url_loader_factory.h"

class ISPWatcher {
 public:
  static void Start();
  static void StartWithURLLoaderFactoryProvider(
      std::function<network::mojom::URLLoaderFactory*()>
          url_loader_factory_provider);
};

#endif  // SERVICES_NETWORK_ISP_WATCHER_H_
