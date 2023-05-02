// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/google_update_util.h"

#include <windows.h>

#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"

#include "build/branding_buildflags.h"  // Needed for REBEL_BROWSER.
#if BUILDFLAG(REBEL_BROWSER)
#include "rebel/build/buildflag.h"
#endif

namespace gcapi_internals {

#if BUILDFLAG(REBEL_BROWSER)
const wchar_t kChromeRegClientsKey[] =
    L"Software\\" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_COMPANY_PATH)
    "\\BrowserUpdate\\Clients\\" REBEL_STRING_BUILDFLAG(REBEL_WINDOWS_APP_GUID);

const wchar_t kChromeRegClientStateKey[] =
    L"Software\\" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_COMPANY_PATH)
    "\\BrowserUpdate\\ClientState\\"
    REBEL_STRING_BUILDFLAG(REBEL_WINDOWS_APP_GUID);
#else
const wchar_t kChromeRegClientsKey[] =
    L"Software\\Google\\Update\\Clients\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kChromeRegClientStateKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
#endif

// Mirror the strategy used by GoogleUpdateSettings::GetBrand.
bool GetBrand(std::wstring* value) {
  const HKEY kRoots[] = {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
  for (HKEY root : kRoots) {
    if (base::win::RegKey(root, kChromeRegClientStateKey,
                          KEY_QUERY_VALUE | KEY_WOW64_32KEY)
            .ReadValue(google_update::kRegBrandField, value) == ERROR_SUCCESS) {
      return true;
    }
  }
  return false;
}

}  // namespace gcapi_internals
