// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific types and constants for Rebel-branded browsers.

#ifndef REBEL_CHROME_INSTALL_STATIC_REBEL_INSTALL_MODES_H_
#define REBEL_CHROME_INSTALL_STATIC_REBEL_INSTALL_MODES_H_

namespace install_static {

// Note: This list of indices must be kept in sync with the brand-specific
// resource strings in chrome/installer/util/prebuild/create_string_rc.
enum InstallConstantIndex {
  REBEL_INDEX,
  NUM_INSTALL_MODES,
};

}  // namespace install_static

#endif  // REBEL_CHROME_INSTALL_STATIC_REBEL_INSTALL_MODES_H_
