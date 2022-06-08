// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_BUILD_BUILDFLAG_H_
#define REBEL_BUILD_BUILDFLAG_H_

#include "build/branding_buildflags.h"

#define REBEL_ESCAPE(...) __VA_ARGS__
#define REBEL_STRIP_PARENTHESES(flag) REBEL_ESCAPE flag

// This is a helper BUILDFLAG macro to remove the double-parentheses surrounding
// the expansion of BUILDFLAG strings. For example:
//
//     BUILDFLAG(REBEL_BROWSER_NAME)
//
// Expands to:
//
//     (("Rebel"))
//
// This inherently makes this expansion invalid in the context of concatenated
// string macros. For example:
//
//     #define FOO BUILDFLAG(REBEL_BROWSER_NAME)
//     const char foo[] = FOO "bar";
//
// Expands to:
//
//     const char foo[] = (("Rebel")) "bar";
//
// Which is invalid C++. The REBEL_STRING_BUILDFLAG macro exapands the BUILDFLAG
// and removes the surrounding parentheses.
#define REBEL_STRING_BUILDFLAG(flag) \
  REBEL_STRIP_PARENTHESES(REBEL_STRIP_PARENTHESES(BUILDFLAG(flag)))

#endif  // REBEL_BUILD_BUILDFLAG_H_
