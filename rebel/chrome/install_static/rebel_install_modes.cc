// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Rebel-branded browsers.

#include "rebel/chrome/install_static/rebel_install_modes.h"

#include <stdlib.h>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/install_static/install_modes.h"

#include "rebel/build/buildflag.h"

namespace install_static {

const wchar_t kCompanyPathName[] =
    L"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_COMPANY_PATH);

const wchar_t kProductPathName[] =
    L"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME);

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const char kSafeBrowsingName[] = REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME);

const InstallConstants kInstallModes[] = {
    // The primary (and only) install mode for stable Rebel-branded browsers.
    {
        .size = sizeof(kInstallModes[0]),

        // The one and only mode for Rebel-branded browsers.
        .index = REBEL_INDEX,

        // No install switch for the primary install mode.
        .install_switch = "",

        // Empty install_suffix for the primary install mode.
        .install_suffix = L"",

        // No logo suffix for the primary install mode.
        .logo_suffix = L"",

        .app_guid = L"" REBEL_STRING_BUILDFLAG(REBEL_WINDOWS_APP_GUID),

        // A distinct base_app_name.
        .base_app_name = L"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME),

        // A distinct base_app_id.
        .base_app_id =
            L"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME_WITH_UNDERSCORES),

        // Browser ProgID prefix.
        .browser_prog_id_prefix =
            L"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_COMPANY_PATH) "HTML",

        // Browser ProgID description
        .browser_prog_id_description =
            L"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME) " HTML Document",

        // PDF ProgID prefix.
        .pdf_prog_id_prefix =
            L"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME) "PDF",

        // PDF ProgID description.
        .pdf_prog_id_description =
            L"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_NAME) " PDF Document",

        // Active Setup GUID.
        .active_setup_guid = L"{7D2B3E1D-D096-4594-9D8F-A6667F12E0AC}",

        // CommandExecuteImpl CLSID.
        .legacy_command_execute_clsid =
            L"{A2DF06F9-A21A-44A8-8A99-8B9C84F29160}",

        // Toast Activator CLSID.
        .toast_activator_clsid = {0x635EFA6F,
                                  0x08D6,
                                  0x4EC9,
                                  {0xBD, 0x14, 0x8A, 0x0F, 0xDE, 0x97, 0x51,
                                   0x59}},

        // Elevator CLSID.
        .elevator_clsid = {0xD133B120,
                           0x6DB4,
                           0x4D6B,
                           {0x8B, 0xFE, 0x83, 0xBF, 0x8C, 0xA1, 0xB1, 0xB0}},

        // IElevator IID and TypeLib {B88C45B9-8825-4629-B83E-77CC67D9CEED}.
        .elevator_iid = {0xb88c45b9,
                         0x8825,
                         0x4629,
                         {0xb8, 0x3e, 0x77, 0xcc, 0x67, 0xd9, 0xce, 0xed}},

        // The empty string means "stable".
        .default_channel_name = L"",
        .channel_strategy = ChannelStrategy::FIXED,

        // Supports system-level installs.
        .supports_system_level = true,

        // Supports in-product set as default browser UX.
        .supports_set_as_default_browser = true,

        // App icon resource index.
        .app_icon_resource_index = icon_resources::kApplicationIndex,

        // App icon resource id.
        .app_icon_resource_id = IDR_MAINFRAME,

        // App container sid prefix for sandbox.
        .sandbox_sid_prefix =
            L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
            L"924012148-",
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
