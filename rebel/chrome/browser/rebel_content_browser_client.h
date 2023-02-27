// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_REBEL_CONTENT_BROWSER_CLIENT_H_
#define REBEL_CHROME_BROWSER_REBEL_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <vector>

#include "chrome/browser/chrome_content_browser_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}  // namespace base

namespace blink {
class AssociatedInterfaceRegistry;
}  // namespace blink

namespace content {
class BrowserContext;
class BrowserMainParts;
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHost;
class RenderProcessHost;
class SiteInstance;
struct Referrer;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace rebel {

class RebelContentBrowserClient : public ChromeContentBrowserClient {
 public:
  RebelContentBrowserClient();
  ~RebelContentBrowserClient() override;

  // content::ContentBrowserClient:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  GURL GetEffectiveURL(content::BrowserContext* browser_context,
                       const GURL& url) override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& site_url) override;
  bool ShouldUseSpareRenderProcessHost(content::BrowserContext* browser_context,
                                       const GURL& site_url) override;
  void OverrideNavigationParams(
      absl::optional<GURL> source_process_site_url,
      ui::PageTransition* transition,
      bool* is_renderer_initiated,
      content::Referrer* referrer,
      absl::optional<url::Origin>* initiator_origin) override;
  bool IsSuitableHost(content::RenderProcessHost* process_host,
                      const GURL& site_url) override;
  void SiteInstanceGotProcessAndSite(
      content::SiteInstance* site_instance) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const absl::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;

 private:
  RebelContentBrowserClient(const RebelContentBrowserClient&) = delete;
  RebelContentBrowserClient& operator=(const RebelContentBrowserClient&) =
      delete;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_REBEL_CONTENT_BROWSER_CLIENT_H_
