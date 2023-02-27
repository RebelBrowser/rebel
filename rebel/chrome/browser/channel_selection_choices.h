// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_CHANNEL_SELECTION_CHOICES_H_
#define REBEL_CHROME_BROWSER_CHANNEL_SELECTION_CHOICES_H_

#include "components/flags_ui/feature_entry.h"

#include "rebel/chrome/browser/channel_selection.h"

namespace rebel {

constexpr const flags_ui::FeatureEntry::Choice kChannelChoices[] = {
    {flags_ui::kGenericExperimentChoiceAutomatic, "", ""},
    {kChannelStable, kChannelCommandLine, kChannelStable},
    {kChannelAlpha, kChannelCommandLine, kChannelAlpha},
    {kChannelPreAlpha, kChannelCommandLine, kChannelPreAlpha},
    {kChannelCanary, kChannelCommandLine, kChannelCanary}};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_CHANNEL_SELECTION_CHOICES_H_
