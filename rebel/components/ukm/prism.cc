// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/prism/prism.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"

#include "rebel/components/ukm/prism_buildflags.h"

namespace rebel {

namespace {
constexpr const char kPrismHintsDisabled[] = "prism-hints-disabled";
constexpr const char kPrismPreconnectOnly[] = "prism-preconnect-only";
constexpr const char kPrismTestLabel[] = "prism-test-label";
}  // namespace

bool IsPrismHintingEnabled() {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kPrismHintsDisabled)) {
    return false;
  }

#if BUILDFLAG(PRISM_ENABLED)
  return true;
#else
  return false;
#endif
}

bool ShouldPrismPreconnectOnly() {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(kPrismPreconnectOnly);
}

void AddViasatMetricsToReport(ukm::Report& report) {
  report.set_viasat_prism_enabled(IsPrismHintingEnabled());

  auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kPrismTestLabel)) {
    auto label = command_line.GetSwitchValueASCII(kPrismTestLabel);
    report.set_viasat_prism_label(std::move(label));
  }

  for (int i = 0; i < report.sources_size(); ++i) {
    ukm::Source* source = report.mutable_sources(i);

    if ((source == nullptr) || !source->has_navigation_time_msec()) {
      continue;
    }

    auto time_since_epoch_ms =
        source->navigation_time_msec() -
        base::TimeTicks::UnixEpoch().since_origin().InMilliseconds();

    time_t time_since_epoch =
        std::chrono::seconds{time_since_epoch_ms / 1000}.count();

    if (const auto* tm = std::gmtime(&time_since_epoch)) {
      std::stringstream formatted_time;
      formatted_time << std::put_time(tm, "%Y-%m-%d %H:%M:%S");

      auto milliseconds = time_since_epoch_ms - time_since_epoch * 1000;
      formatted_time << '.' << std::setw(3) << std::setfill('0')
                     << milliseconds;

      source->set_viasat_navigation_start_time(formatted_time.str());
    }
  }
}

}  // namespace rebel
