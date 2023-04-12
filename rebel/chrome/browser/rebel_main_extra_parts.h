// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts.h"

#ifndef REBEL_CHROME_BROWSER_REBEL_MAIN_EXTRA_PARTS_H_
#define REBEL_CHROME_BROWSER_REBEL_MAIN_EXTRA_PARTS_H_

class Profile;

namespace rebel {

class RebelMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  // ChromeBrowserMainExtraParts:
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_REBEL_MAIN_EXTRA_PARTS_H_
