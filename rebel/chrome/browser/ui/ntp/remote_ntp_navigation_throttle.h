// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_NAVIGATION_THROTTLE_H_
#define REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace rebel {

// A NavigationThrottle that opens the offline RemoteNTP when there is any issue
// opening the online RemoteNTP.
class RemoteNtpNavigationThrottle : public content::NavigationThrottle {
 public:
  // Returns a NavigationThrottle when:
  // - we are navigating to the new tab page, and
  // - the main frame is pointed at the new tab URL.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  explicit RemoteNtpNavigationThrottle(content::NavigationHandle* handle);
  ~RemoteNtpNavigationThrottle() override;

  // Overridden from content::NavigationThrottle:
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult OpenOfflineRemoteNtp();
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_UI_NTP_REMOTE_NTP_NAVIGATION_THROTTLE_H_
