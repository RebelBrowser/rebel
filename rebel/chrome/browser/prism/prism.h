// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_PRISM_PRISM_H_
#define REBEL_CHROME_BROWSER_PRISM_PRISM_H_

class Profile;

namespace rebel {

void InitializeCommandLineForPrism();
void InitializeProfileForPrism(Profile&);

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_PRISM_PRISM_H_
