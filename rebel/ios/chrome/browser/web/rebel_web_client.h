// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_WEB_REBEL_WEB_CLIENT_H_
#define REBEL_IOS_CHROME_BROWSER_WEB_REBEL_WEB_CLIENT_H_

#include "ios/chrome/browser/web/model/chrome_web_client.h"

namespace rebel {

// Rebel implementation of WebClient.
class RebelWebClient : public ChromeWebClient {
 public:
  RebelWebClient();
  ~RebelWebClient() override;

  // ChromeWebClient:
  void AddAdditionalSchemes(Schemes* schemes) const override;

 private:
  RebelWebClient(const RebelWebClient&) = delete;
  RebelWebClient& operator=(const RebelWebClient&) = delete;
};

}  // namespace rebel

#endif  // REBEL_IOS_CHROME_BROWSER_WEB_REBEL_WEB_CLIENT_H_
