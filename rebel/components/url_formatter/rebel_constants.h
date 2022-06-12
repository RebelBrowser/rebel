// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_COMPONENTS_URL_FORMATTER_REBEL_CONSTANTS_H_
#define REBEL_COMPONENTS_URL_FORMATTER_REBEL_CONSTANTS_H_

#include <string>

class GURL;

namespace rebel {

extern const char kRebelScheme[];

bool ReplaceChromeSchemeWithRebelScheme(GURL& url);
bool ReplaceRebelSchemeWithChromeScheme(GURL& url);

bool ReplaceChromeSchemeWithRebelScheme(std::u16string& url);

bool SchemeIsRebelOrChrome(const GURL& url);

}  // namespace rebel

#endif  // REBEL_COMPONENTS_URL_FORMATTER_REBEL_CONSTANTS_H_
