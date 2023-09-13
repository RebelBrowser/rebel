// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_COMPONENTS_UKM_PRISM_H_
#define REBEL_COMPONENTS_UKM_PRISM_H_

namespace ukm {
class Report;
}  // namespace ukm

namespace rebel {

bool IsPrismHintingEnabled();
bool ShouldPrismPreconnectOnly();

void AddViasatMetricsToReport(ukm::Report& report);

}  // namespace rebel

#endif  // REBEL_COMPONENTS_UKM_PRISM_H_
