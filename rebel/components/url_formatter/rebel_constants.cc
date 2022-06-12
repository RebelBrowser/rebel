// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/components/url_formatter/rebel_constants.h"

#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "url/gurl.h"

#include "rebel/build/buildflag.h"

namespace rebel {

namespace {

// The same as content::kChromeUIScheme, but we cannot depend on //content here.
constexpr const char kChromeScheme[] = "chrome";

bool SwapSchemas(GURL& url,
                 base::StringPiece source_schema,
                 base::StringPiece replacement_schema) {
  if (!url.SchemeIs(source_schema)) {
    return false;
  }

  GURL::Replacements replacements;
  replacements.SetSchemeStr(replacement_schema);

  url = url.ReplaceComponents(replacements);
  return true;
}

}  // namespace

const char kRebelScheme[] = BUILDFLAG(REBEL_BROWSER_SCHEMA);

bool ReplaceChromeSchemeWithRebelScheme(GURL& url) {
  return SwapSchemas(url, kChromeScheme, kRebelScheme);
}

bool ReplaceRebelSchemeWithChromeScheme(GURL& url) {
  return SwapSchemas(url, kRebelScheme, kChromeScheme);
}

bool ReplaceChromeSchemeWithRebelScheme(std::u16string& url) {
  static constexpr const base::StringPiece16 chrome_scheme_with_separator =
      u"chrome://";
  static constexpr const base::StringPiece16 rebel_scheme_with_separator =
      u"" REBEL_STRING_BUILDFLAG(REBEL_BROWSER_SCHEMA) "://";

  if (!base::StartsWith(url, chrome_scheme_with_separator)) {
    return false;
  }

  base::ReplaceFirstSubstringAfterOffset(&url, 0, chrome_scheme_with_separator,
                                         rebel_scheme_with_separator);
  return true;
}

bool SchemeIsRebelOrChrome(const GURL& url) {
  return url.SchemeIs(kRebelScheme) || url.SchemeIs(kChromeScheme);
}

}  // namespace rebel
