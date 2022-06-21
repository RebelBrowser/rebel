// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/common/rebel_content_client.h"

#include "rebel/components/url_formatter/rebel_constants.h"

namespace rebel {

RebelContentClient::RebelContentClient() : ChromeContentClient() {}
RebelContentClient::~RebelContentClient() = default;

void RebelContentClient::AddAdditionalSchemes(Schemes* schemes) {
  ChromeContentClient::AddAdditionalSchemes(schemes);

  schemes->standard_schemes.push_back(rebel::kRebelScheme);
  schemes->secure_schemes.push_back(rebel::kRebelScheme);
  schemes->cors_enabled_schemes.push_back(rebel::kRebelScheme);
  schemes->savable_schemes.push_back(rebel::kRebelScheme);
}

}  // namespace rebel
