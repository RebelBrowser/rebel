// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ui/ntp/remote_ntp_navigation_throttle.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/browser/ntp/remote_ntp_service_factory.h"
#include "rebel/chrome/common/ntp/remote_ntp_prefs.h"

namespace rebel {

RemoteNtpNavigationThrottle::RemoteNtpNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

RemoteNtpNavigationThrottle::~RemoteNtpNavigationThrottle() = default;

const char* RemoteNtpNavigationThrottle::GetNameForLogging() {
  return "RemoteNtpNavigationThrottle";
}

// static
std::unique_ptr<content::NavigationThrottle>
RemoteNtpNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  rebel::RemoteNtpService* remote_ntp_service =
      rebel::RemoteNtpServiceFactory::GetForProfile(profile);

  if (!remote_ntp_service ||
      !remote_ntp_service->IsRemoteNtpUrl(handle->GetURL()) ||
      handle->GetURL() == rebel::kRemoteNtpOfflineUrl) {
    return nullptr;
  }

  return std::make_unique<RemoteNtpNavigationThrottle>(handle);
}

content::NavigationThrottle::ThrottleCheckResult
RemoteNtpNavigationThrottle::WillProcessResponse() {
  const net::HttpResponseHeaders* headers =
      navigation_handle()->GetResponseHeaders();
  if (!headers) {
    return content::NavigationThrottle::PROCEED;
  }

  int response_code = headers->response_code();
  if ((response_code < 400) && (response_code != net::HTTP_NO_CONTENT)) {
    return content::NavigationThrottle::PROCEED;
  }

  return OpenOfflineRemoteNtp();
}

content::NavigationThrottle::ThrottleCheckResult
RemoteNtpNavigationThrottle::WillFailRequest() {
  return OpenOfflineRemoteNtp();
}

content::NavigationThrottle::ThrottleCheckResult
RemoteNtpNavigationThrottle::OpenOfflineRemoteNtp() {
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.url = GURL(rebel::kRemoteNtpOfflineUrl);
  params.is_renderer_initiated = false;

  navigation_handle()->GetWebContents()->OpenURL(std::move(params), {});

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

}  // namespace rebel
