// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_DELEGATE_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_DELEGATE_H_

namespace rebel {

// Delegate for the RemoteNtpThemeProvider. This is defined in a separate file
// from the RemoteNtpThemeProvider class because that class is only available
// on desktop; defining this delegate here allows classes to implement the
// interface without a mess of OS-type #ifdef's.
class RemoteNtpThemeDelegate {
 public:
  // Called when the browser or system theme has changed in any way.
  virtual void OnThemeUpdated() {}
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_THEME_DELEGATE_H_
