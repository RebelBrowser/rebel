// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_COMMON_REBEL_CONTENT_CLIENT_H_
#define REBEL_CHROME_COMMON_REBEL_CONTENT_CLIENT_H_

#include "chrome/common/chrome_content_client.h"

namespace rebel {

class RebelContentClient : public ChromeContentClient {
 public:
  RebelContentClient();
  ~RebelContentClient() override;

  // ChromeContentClient:
  void AddAdditionalSchemes(Schemes* schemes) override;

 private:
  RebelContentClient(const RebelContentClient&) = delete;
  RebelContentClient& operator=(const RebelContentClient&) = delete;
};

}  // namespace rebel

#endif  // REBEL_CHROME_COMMON_REBEL_CONTENT_CLIENT_H_
