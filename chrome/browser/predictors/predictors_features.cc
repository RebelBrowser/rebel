// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictors_features.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#include "build/branding_buildflags.h"  // Needed for REBEL_BROWSER.
#if BUILDFLAG(REBEL_BROWSER)
#include "rebel/components/ukm/prism_buildflags.h"
#endif

namespace features {

// Whether local predictions should be used to make preconnect predictions.
BASE_FEATURE(kLoadingPredictorUseLocalPredictions,
             "LoadingPredictorUseLocalPredictions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Modifies loading predictor so that it only learns about subresources and
// origins that are high priority.
BASE_FEATURE(kLoadingOnlyLearnHighPriorityResources,
             "LoadingOnlyLearnHighPriorityResources",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Configures the loading predictor table size and other base parameters.
BASE_FEATURE(kLoadingPredictorTableConfig,
             "LoadingPredictorTableConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Modifies loading predictor so that the predictions also contain origins of
// the redirect target of the navigation.
BASE_FEATURE(kLoadingPreconnectToRedirectTarget,
             "LoadingPreconnectToRedirectTarget",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Modifies loading predictor so that the value of the |always_access_network|
// attribute is not used when computing the predicting score for an origin.
BASE_FEATURE(kLoadingPredictorDisregardAlwaysAccessesNetwork,
             "LoadingPredictorDisregardAlwaysAccessesNetwork",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureState
    kLoadingPredictorUseOptimizationGuideDefaultFeatureState =
#if BUILDFLAG(IS_ANDROID)
        base::FEATURE_ENABLED_BY_DEFAULT;
#else
        base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// Modifies loading predictor so that it can also use predictions coming from
// the optimization guide.
BASE_FEATURE(kLoadingPredictorUseOptimizationGuide,
             "LoadingPredictorUseOptimizationGuide",
             kLoadingPredictorUseOptimizationGuideDefaultFeatureState);

const base::FeatureState kLoadingPredictorPrefetchDefaultFeatureState =
#if BUILDFLAG(IS_ANDROID)
    base::FEATURE_ENABLED_BY_DEFAULT;
#else
    base::FEATURE_DISABLED_BY_DEFAULT;
#endif

// Modifies loading predictor so that it does prefetches of subresources instead
// of preconnects.
BASE_FEATURE(kLoadingPredictorPrefetch,
             "LoadingPredictorPrefetch",
             kLoadingPredictorPrefetchDefaultFeatureState);

const base::FeatureParam<PrefetchSubresourceType>::Option
#if BUILDFLAG(REBEL_BROWSER)
    kPrefetchSubresourceTypeParamOptions[] = {
        {PrefetchSubresourceType::kAll, "all"},
        {PrefetchSubresourceType::kCss, "css"},
        {PrefetchSubresourceType::kJs, "js"},
        {PrefetchSubresourceType::kJsAndCss, "js_css"},
        {PrefetchSubresourceType::kImg, "img"},
        {PrefetchSubresourceType::kFont, "font"},
        {PrefetchSubresourceType::kVdo, "video"},
        {PrefetchSubresourceType::kCssFont, "css_font"},
        {PrefetchSubresourceType::kCssImg, "css_img"},
        {PrefetchSubresourceType::kCssFontJs, "css_font_js"},
        {PrefetchSubresourceType::kCssFontJsImg, "css_font_js_img"},
        {PrefetchSubresourceType::kPreconnect_only, "preconnect_only"}};
#else
    kPrefetchSubresourceTypeParamOptions[] = {
        {PrefetchSubresourceType::kAll, "all"},
        {PrefetchSubresourceType::kCss, "css"},
        {PrefetchSubresourceType::kJsAndCss, "js_css"}};
#endif
const base::FeatureParam<PrefetchSubresourceType>
    kLoadingPredictorPrefetchSubresourceType{
        &kLoadingPredictorPrefetch, "subresource_type",
        PrefetchSubresourceType::kAll, &kPrefetchSubresourceTypeParamOptions};

BASE_FEATURE(kLoadingPredictorInflightPredictiveActions,
             "kLoadingPredictorInflightPredictiveActions",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool ShouldUseLocalPredictions() {
  return base::FeatureList::IsEnabled(kLoadingPredictorUseLocalPredictions);
}

bool ShouldUseOptimizationGuidePredictions() {
  if (!base::FeatureList::IsEnabled(kLoadingPredictorUseOptimizationGuide))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      kLoadingPredictorUseOptimizationGuide, "use_predictions", true);
}

bool ShouldAlwaysRetrieveOptimizationGuidePredictions() {
#if BUILDFLAG(PRISM_ENABLED)
  return true;
#else
  return base::GetFieldTrialParamByFeatureAsBool(
      kLoadingPredictorUseOptimizationGuide, "always_retrieve_predictions",
      false);
#endif
}

size_t GetMaxInflightPrefetches() {
#if BUILDFLAG(PRISM_ENABLED)
  return 100;
#else
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kLoadingPredictorInflightPredictiveActions, "max_inflight_prefetches",
      3));
#endif
}

}  // namespace features
