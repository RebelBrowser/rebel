// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_REBEL_MAIN_EXTRA_PARTS_H_
#define REBEL_CHROME_BROWSER_REBEL_MAIN_EXTRA_PARTS_H_

#include <memory>

#include "chrome/browser/chrome_browser_main_extra_parts.h"

class Profile;

namespace rebel {

class ISPWatcher;

class RebelMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  RebelMainExtraParts();
  ~RebelMainExtraParts() override;

  // ChromeBrowserMainExtraParts:
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;

 private:
  std::unique_ptr<rebel::ISPWatcher> rebel_isp_watcher_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_REBEL_MAIN_EXTRA_PARTS_H_
