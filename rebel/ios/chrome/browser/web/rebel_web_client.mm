// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/ios/chrome/browser/web/rebel_web_client.h"

#include "rebel/components/url_formatter/rebel_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace rebel {

RebelWebClient::RebelWebClient() : ChromeWebClient() {}
RebelWebClient::~RebelWebClient() = default;

void RebelWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  ChromeWebClient::AddAdditionalSchemes(schemes);

  schemes->standard_schemes.push_back(rebel::kRebelScheme);
  schemes->secure_schemes.push_back(rebel::kRebelScheme);
}

}  // namespace rebel
