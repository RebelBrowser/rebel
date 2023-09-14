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
#include "components/optimization_guide/core/optimization_guide_features.h"
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

void AppendFeature(std::vector<base::StringPiece>& features,
                   base::Feature const& feature_to_append) {
  for (auto feature : features) {
    // Features with parameterized values will be of the form:
    // --enable-features="LoadingPredictorPrefetch:subresource_type/css"
    if (auto index = feature.find(':'); index != base::StringPiece::npos) {
      feature = feature.substr(0, index);
    }

    if (feature == feature_to_append.name) {
      return;
    }
  }

  features.push_back(feature_to_append.name);
}

}  // namespace

// If Prism hinting is enabled, we:
//   1. Enable LoadingPredictor use of Optimization Guide
//   2. Enable Optimization Guide use of hinting
//   3. If --prism-preconnect-only is on the command line:
//        a. Disable LoadingPredictor use of prefetching
//      Otherwise:
//        b. Enable LoadingPredictor use of prefetching
//   4. Set the Optimization Guide hints URL to Prism
//
// If Prism hinting is disabled, we:
//   1. Disable LoadingPredictor use of Optimization Guide
//   2. Disable LoadingPredictor use of prefetch
//   3. Disable Optimization Guide use of hinting
//
// Regardless of whether Prism hinting is enabled, we:
//   1. Disable Optimization Guide use of remote model fetching
//   2. Set the UKM URL to Prism
void InitializeCommandLineForPrism() {
  auto& command_line = *base::CommandLine::ForCurrentProcess();

  auto enabled_features_flag =
      command_line.GetSwitchValueASCII(switches::kEnableFeatures);
  auto enabled_features =
      base::FeatureList::SplitFeatureListString(enabled_features_flag);

  auto disabled_features_flag =
      command_line.GetSwitchValueASCII(switches::kDisableFeatures);
  auto disabled_features =
      base::FeatureList::SplitFeatureListString(disabled_features_flag);

  if (IsPrismHintingEnabled()) {
    AppendFeature(enabled_features,
                  features::kLoadingPredictorUseOptimizationGuide);
    AppendFeature(
        enabled_features,
        optimization_guide::features::kRemoteOptimizationGuideFetching);

    if (ShouldPrismPreconnectOnly()) {
      AppendFeature(disabled_features, features::kLoadingPredictorPrefetch);
    } else {
      AppendFeature(enabled_features, features::kLoadingPredictorPrefetch);
    }

    if (!command_line.HasSwitch(optimization_guide::switches::
                                    kOptimizationGuideServiceGetHintsURL)) {
      command_line.AppendSwitchASCII(
          optimization_guide::switches::kOptimizationGuideServiceGetHintsURL,
          kPrismHintsURL);
    }
  } else {
    AppendFeature(disabled_features, features::kLoadingPredictorPrefetch);
    AppendFeature(disabled_features,
                  features::kLoadingPredictorUseOptimizationGuide);
    AppendFeature(
        disabled_features,
        optimization_guide::features::kRemoteOptimizationGuideFetching);
  }

  AppendFeature(
      disabled_features,
      optimization_guide::features::kOptimizationGuideModelDownloading);

  if (!enabled_features.empty()) {
    std::string enabled = JoinCSVList(enabled_features);
    command_line.AppendSwitchASCII(switches::kEnableFeatures, enabled);
  }
  if (!disabled_features.empty()) {
    std::string disabled = JoinCSVList(disabled_features);
    command_line.AppendSwitchASCII(switches::kDisableFeatures, disabled);
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
