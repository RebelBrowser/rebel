// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/browser/ntp/remote_ntp_search_provider.h"

#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

#include "rebel/chrome/browser/ntp/remote_ntp_service.h"
#include "rebel/chrome/common/ntp/remote_ntp.mojom.h"

namespace rebel {

namespace {

std::vector<rebel::mojom::AutocompleteMatchPtr>
ConvertAutocompleteMatchesToMojo(const AutocompleteResult& result) {
  std::vector<rebel::mojom::AutocompleteMatchPtr> matches;

  for (const AutocompleteMatch& raw_match : result) {
    auto mojo_match = rebel::mojom::AutocompleteMatch::New();
    const AutocompleteMatch match =
        raw_match.GetMatchWithContentsAndDescriptionPossiblySwapped();

    mojo_match->contents = match.contents;
    for (const auto& contents_class : match.contents_class) {
      mojo_match->contents_class.push_back(
          rebel::mojom::ACMatchClassification::New(contents_class.offset,
                                                   contents_class.style));
    }

    mojo_match->description = match.description;
    for (const auto& description_class : match.description_class) {
      mojo_match->description_class.push_back(
          rebel::mojom::ACMatchClassification::New(description_class.offset,
                                                   description_class.style));
    }

    mojo_match->destination_url = match.destination_url.spec();

    mojo_match->type = AutocompleteMatchType::ToString(match.type);
    mojo_match->is_search_type = AutocompleteMatch::IsSearchType(match.type);

    mojo_match->fill_into_edit = match.fill_into_edit;
    mojo_match->inline_autocompletion = match.inline_autocompletion;
    mojo_match->allowed_to_be_default_match = match.allowed_to_be_default_match;

    matches.push_back(std::move(mojo_match));
  }

  return matches;
}

}  // namespace

RemoteNtpSearchProvider::RemoteNtpSearchProvider(Delegate* delegate)
    : delegate_(delegate) {}

RemoteNtpSearchProvider::~RemoteNtpSearchProvider() = default;

void RemoteNtpSearchProvider::OnResultChanged(
    AutocompleteController* controller,
    bool default_result_changed) {
  if (!autocomplete_controller_) {
    return;
  }

  auto result = rebel::mojom::AutocompleteResult::New(
      autocomplete_controller_->input().text(),
      ConvertAutocompleteMatchesToMojo(autocomplete_controller_->result()));

  delegate_->OnAutocompleteResultChanged(std::move(result));
}

void RemoteNtpSearchProvider::QueryAutocomplete(
    rebel::RemoteNtpService* remote_ntp_service,
    const std::u16string& input,
    bool prevent_inline_autocomplete,
    const AutocompleteSchemeClassifier& scheme_classifier) {
  if (!autocomplete_controller_ && remote_ntp_service) {
    autocomplete_controller_ =
        remote_ntp_service->CreateAutocompleteController();
    autocomplete_controller_->AddObserver(this);
  }
  if (!autocomplete_controller_) {
    return;
  }

  if (time_of_first_autocomplete_query_.is_null() && !input.empty()) {
    time_of_first_autocomplete_query_ = base::TimeTicks::Now();
  }

  AutocompleteInput autocomplete_input(
      input, metrics::OmniboxEventProto::NTP_REALBOX, scheme_classifier);
  autocomplete_input.set_focus_type(
      input.empty() ? metrics::OmniboxFocusType::INTERACTION_FOCUS
                    : metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  autocomplete_input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete);
  autocomplete_input.set_prefer_keyword(false);
  autocomplete_input.set_allow_exact_keyword_match(false);
  autocomplete_controller_->Start(autocomplete_input);
}

void RemoteNtpSearchProvider::StopAutocomplete() {
  if (!autocomplete_controller_) {
    return;
  }

  autocomplete_controller_->Stop(true);
  time_of_first_autocomplete_query_ = base::TimeTicks();
}

bool RemoteNtpSearchProvider::MatchSelected(
    uint32_t index,
    const GURL& url,
    GURL& destination_url,
    ui::PageTransition& transition_type) {
  if (!autocomplete_controller_) {
    return false;
  } else if (index >= autocomplete_controller_->result().size()) {
    return false;
  }

  AutocompleteMatch match(autocomplete_controller_->result().match_at(index));
  if (url != match.destination_url) {
    return false;
  }

  // Update the selected match with parameters that were not available when the
  // match was generated.
  const auto now = base::TimeTicks::Now();
  const base::TimeDelta elapsed_time = now - time_of_first_autocomplete_query_;
  autocomplete_controller_
      ->UpdateMatchDestinationURLWithAdditionalSearchboxStats(elapsed_time,
                                                              &match);

  destination_url = match.destination_url;
  transition_type = match.transition;

  return destination_url.is_valid();
}

void RemoteNtpSearchProvider::DidStartNavigation() {
  time_of_first_autocomplete_query_ = base::TimeTicks();
}

}  // namespace rebel
