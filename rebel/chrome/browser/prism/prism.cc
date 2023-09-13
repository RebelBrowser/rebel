// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/prism/prism.h"

#include <set>
#include <sstream>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/ukm/ukm_service.h"
#include "components/unified_consent/unified_consent_service.h"

#include "rebel/components/ukm/prism.h"
#include "rebel/components/ukm/prism_buildflags.h"

namespace rebel {

namespace {

constexpr const char kPrismHintsURL[] = BUILDFLAG(PRISM_HINTS_URL);
constexpr const char kPrismMetricsUrl[] = BUILDFLAG(PRISM_METRICS_URL);

template <typename ContainerType>
std::string JoinCSVList(const ContainerType& list) {
  std::stringstream stream;

  for (auto it = list.cbegin(); it != list.cend(); ++it) {
    if (it != list.cbegin()) {
      stream << ',';
    }

    stream << *it;
  }

  return stream.str();
}

}  // namespace

void InitializeCommandLineForPrism() {
  auto& command_line = *base::CommandLine::ForCurrentProcess();

  if (IsPrismHintingEnabled()) {
    std::vector<base::StringPiece> prism_features{
        features::kLoadingPredictorUseOptimizationGuide.name,
    };

    if (!ShouldPrismPreconnectOnly()) {
      prism_features.push_back(features::kLoadingPredictorPrefetch.name);
    }

    auto enabled_features_flag =
        command_line.GetSwitchValueASCII(switches::kEnableFeatures);
    auto enabled_features =
        base::FeatureList::SplitFeatureListString(enabled_features_flag);

    for (auto const& prism_feature : prism_features) {
      bool feature_already_enabled = false;

      for (auto enabled_feature : enabled_features) {
        // Features with parameterized values will be of the form:
        // --enable-features="LoadingPredictorPrefetch:subresource_type/css"
        if (auto index = enabled_feature.find(':');
            index != base::StringPiece::npos) {
          enabled_feature = enabled_feature.substr(0, index);
        }

        if (enabled_feature == prism_feature) {
          feature_already_enabled = true;
          break;
        }
      }

      if (!feature_already_enabled) {
        enabled_features.push_back(prism_feature);
      }
    }

    std::string enabled = JoinCSVList(enabled_features);
    command_line.AppendSwitchASCII(switches::kEnableFeatures, enabled);

    if (!command_line.HasSwitch(optimization_guide::switches::
                                    kOptimizationGuideServiceGetHintsURL)) {
      command_line.AppendSwitchASCII(
          optimization_guide::switches::kOptimizationGuideServiceGetHintsURL,
          kPrismHintsURL);
    }
  }

  if (!command_line.HasSwitch(metrics::switches::kUkmServerUrl)) {
    command_line.AppendSwitchASCII(metrics::switches::kUkmServerUrl,
                                   kPrismMetricsUrl);
  }
}

void InitializeProfileForPrism(Profile& profile) {
  if (profile.IsOffTheRecord()) {
    return;
  }

  if (auto* service = UnifiedConsentServiceFactory::GetForProfile(&profile)) {
    service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  }
}

OptimizationGuideLogger::LogMessageBuilder OptimizationGuideLogger(
    Profile* profile) {
  static constexpr auto source =
      optimization_guide_common::mojom::LogSource::HINTS;

  if (!profile || !IsPrismHintingEnabled()) {
    return OPTIMIZATION_GUIDE_LOGGER(source, nullptr);
  }

  auto* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_service) {
    return OPTIMIZATION_GUIDE_LOGGER(source, nullptr);
  }

  auto* optimization_guide_logger =
      optimization_guide_service->GetOptimizationGuideLogger();
  return OPTIMIZATION_GUIDE_LOGGER(source, optimization_guide_logger);
}

}  // namespace rebel
