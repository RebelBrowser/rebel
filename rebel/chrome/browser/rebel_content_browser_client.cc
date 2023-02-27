// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/rebel_content_browser_client.h"

#include <string>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "url/origin.h"

#include "rebel/chrome/browser/ntp/remote_ntp_icon_receiver.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_impl.h"
#include "rebel/chrome/browser/rebel_main_extra_parts.h"
#include "rebel/chrome/browser/ui/ntp/remote_ntp_navigation_throttle.h"
#include "rebel/chrome/browser/ui/ntp/remote_ntp_tab_helper.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

namespace rebel {

RebelContentBrowserClient::RebelContentBrowserClient()
    : ChromeContentBrowserClient() {}

RebelContentBrowserClient::~RebelContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
RebelContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  auto main_parts =
      ChromeContentBrowserClient::CreateBrowserMainParts(is_integration_test);

  auto& chrome_parts = reinterpret_cast<ChromeBrowserMainParts&>(*main_parts);
  chrome_parts.AddParts(std::make_unique<RebelMainExtraParts>());

  return main_parts;
}

GURL RebelContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  auto* profile = Profile::FromBrowserContext(browser_context);

  // If the input |url| should be assigned to the RemoteNTP renderer, make its
  // effective URL distinct from other URLs on the provider's domain.
  if (RemoteNtpServiceImpl::ShouldAssignUrlToRemoteNtpRenderer(url, profile)) {
    return RemoteNtpServiceImpl::GetEffectiveURLForRemoteNtp(url);
  }
  if (RemoteNtpServiceImpl::ShouldAllowUrlToUseRemoteNtpAPI(url, profile)) {
    return RemoteNtpServiceImpl::GetEffectiveURLForRemoteNtpAPI(url);
  }

  return ChromeContentBrowserClient::GetEffectiveURL(browser_context, url);
}

bool RebelContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  auto* profile = Profile::FromBrowserContext(browser_context);

  if (RemoteNtpServiceImpl::ShouldUseProcessPerSiteForRemoteNtpUrl(site_url,
                                                                   profile)) {
    return true;
  }

  return ChromeContentBrowserClient::ShouldUseProcessPerSite(browser_context,
                                                             site_url);
}

bool RebelContentBrowserClient::ShouldUseSpareRenderProcessHost(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  auto* profile = Profile::FromBrowserContext(browser_context);

  // RemoteNTP renderers should not use a spare process, because they require
  // passing kRemoteNtpProcess to the renderer process when it launches.
  // A spare process is launched earlier, before it is known which navigation
  // will use it, so it lacks this flag.
  bool is_remote_ntp = RemoteNtpServiceImpl::ShouldAssignUrlToRemoteNtpRenderer(
      site_url, profile);
  bool is_allowed_to_use_api =
      RemoteNtpServiceImpl::ShouldAllowUrlToUseRemoteNtpAPI(site_url, profile);

  if (is_remote_ntp || is_allowed_to_use_api) {
    return false;
  }

  return ChromeContentBrowserClient::ShouldUseSpareRenderProcessHost(
      browser_context, site_url);
}

void RebelContentBrowserClient::OverrideNavigationParams(
    absl::optional<GURL> source_process_site_url,
    ui::PageTransition* transition,
    bool* is_renderer_initiated,
    content::Referrer* referrer,
    absl::optional<url::Origin>* initiator_origin) {
  if (source_process_site_url &&
      RemoteNtpService::IsRemoteNtpUrl(*source_process_site_url) &&
      ui::PageTransitionCoreTypeIs(*transition, ui::PAGE_TRANSITION_LINK)) {
    // Clicks on tiles of the new tab page should be treated as if a user
    // clicked on a bookmark. This is consistent with native implementations
    // like Android's. This also helps ensure that security features (like
    // Sec-Fetch-Site and SameSite-cookies) will treat the navigation as
    // browser-initiated.
    *transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    *is_renderer_initiated = false;
    *referrer = content::Referrer();
    *initiator_origin = absl::nullopt;
  }

  ChromeContentBrowserClient::OverrideNavigationParams(
      std::move(source_process_site_url), transition, is_renderer_initiated,
      referrer, initiator_origin);
}

bool RebelContentBrowserClient::IsSuitableHost(
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  auto* profile =
      Profile::FromBrowserContext(process_host->GetBrowserContext());

  auto* remote_ntp_service = RemoteNtpServiceFactory::GetForProfile(profile);

  // RemoteNTP URLs should only be in the RemoteNTP process and RemoteNTP
  // process should only have RemoteNTP URLs.
  if (remote_ntp_service) {
    bool is_remote_ntp_process =
        remote_ntp_service->IsRemoteNtpProcess(process_host->GetID());
    bool should_be_in_remote_ntp_process =
        RemoteNtpServiceImpl::ShouldAssignUrlToRemoteNtpRenderer(site_url,
                                                                 profile);

    if (is_remote_ntp_process || should_be_in_remote_ntp_process) {
      return is_remote_ntp_process && should_be_in_remote_ntp_process;
    }
  }

  return ChromeContentBrowserClient::IsSuitableHost(process_host, site_url);
}

void RebelContentBrowserClient::SiteInstanceGotProcessAndSite(
    content::SiteInstance* site_instance) {
  auto* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  const GURL& site_url = site_instance->GetSiteURL();

  // Remember the ID of the RemoteNTP process to signal the renderer process
  // on startup in |AppendExtraCommandLineSwitches| below.
  bool is_remote_ntp = RemoteNtpServiceImpl::ShouldAssignUrlToRemoteNtpRenderer(
      site_url, profile);
  bool is_allowed_to_use_api =
      RemoteNtpServiceImpl::ShouldAllowUrlToUseRemoteNtpAPI(site_url, profile);

  if (is_remote_ntp || is_allowed_to_use_api) {
    auto* remote_ntp_service = RemoteNtpServiceFactory::GetForProfile(profile);
    int process_id = site_instance->GetProcess()->GetID();

    if (remote_ntp_service) {
      if (is_remote_ntp) {
        remote_ntp_service->AddRemoteNtpProcess(process_id);
      } else {
        remote_ntp_service->AddRemoteNtpAPIProcess(process_id);
      }
    }
  }

  ChromeContentBrowserClient::SiteInstanceGotProcessAndSite(site_instance);
}

void RebelContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    auto* process = content::RenderProcessHost::FromID(child_process_id);
    auto* profile =
        process ? Profile::FromBrowserContext(process->GetBrowserContext())
                : nullptr;

    auto* remote_ntp_service = RemoteNtpServiceFactory::GetForProfile(profile);
    if (remote_ntp_service) {
      int process_id = process->GetID();

      if (remote_ntp_service->IsRemoteNtpProcess(process_id) ||
          remote_ntp_service->IsRemoteNtpAPIProcess(process_id)) {
        command_line->AppendSwitch(rebel::kRemoteNtpProcess);
      }
    }
  }

  ChromeContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                             child_process_id);
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
RebelContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  auto throttles =
      ChromeContentBrowserClient::CreateThrottlesForNavigation(handle);

  // This must be inserted *before* NewTabPageNavigationThrottle, otherwise the
  // Chromium local NTP will be loaded instead of the RemoteNTP.
  auto throttle = RemoteNtpNavigationThrottle::MaybeCreateThrottleFor(handle);
  if (throttle) {
    throttles.insert(throttles.begin(), std::move(throttle));
  }

  return throttles;
}

void RebelContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<rebel::mojom::RemoteNtpConnector>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<rebel::mojom::RemoteNtpConnector>
                 receiver) {
            rebel::RemoteNtpTabHelper::BindRemoteNtpConnector(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));

  ChromeContentBrowserClient::
      RegisterAssociatedInterfaceBindersForRenderFrameHost(render_frame_host,
                                                           associated_registry);
}

void RebelContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<rebel::mojom::RemoteNtpIconReceiver>(
      base::BindRepeating(&rebel::RemoteNtpIconReceiver::Create));

  ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);
}

void RebelContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    const absl::optional<url::Origin>& request_initiator_origin,
    NonNetworkURLLoaderFactoryMap* factories) {
  auto* frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);

  if (web_contents) {
    auto* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    auto* remote_ntp_service =
        rebel::RemoteNtpServiceFactory::GetForProfile(profile);

    if (remote_ntp_service &&
        remote_ntp_service->IsRemoteNtpProcess(render_process_id)) {
      factories->emplace(
          chrome::kChromeSearchScheme,
          content::CreateWebUIURLLoaderFactory(
              frame_host, chrome::kChromeSearchScheme,
              /*allowed_webui_hosts=*/base::flat_set<std::string>()));
      factories->emplace(
          content::kChromeUIUntrustedScheme,
          content::CreateWebUIURLLoaderFactory(
              frame_host, content::kChromeUIUntrustedScheme,
              /*allowed_webui_hosts=*/base::flat_set<std::string>()));
    }
  }

  ChromeContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
      render_process_id, render_frame_id, request_initiator_origin, factories);
}

}  // namespace rebel
