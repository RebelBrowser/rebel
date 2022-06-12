// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SEARCH_PROVIDER_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SEARCH_PROVIDER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "ui/base/page_transition_types.h"

#include "rebel/chrome/common/ntp/remote_ntp.mojom-forward.h"

class AutocompleteController;
class AutocompleteSchemeClassifier;
class GURL;

namespace rebel {

class RemoteNtpService;

// Platform-independent abstraction around the autocomplete component for
// issuing autocomplete queries and receiving results.
class RemoteNtpSearchProvider : public AutocompleteController::Observer {
 public:
  // Delegate to receieve autocomplete results.
  class Delegate {
   public:
    // Called when the current autocomplete result has changed.
    virtual void OnAutocompleteResultChanged(
        rebel::mojom::AutocompleteResultPtr result) = 0;
  };

  explicit RemoteNtpSearchProvider(Delegate* delegate);
  ~RemoteNtpSearchProvider() override;

  void QueryAutocomplete(rebel::RemoteNtpService* remote_ntp_service,
                         const std::u16string& input,
                         bool prevent_inline_autocomplete,
                         const AutocompleteSchemeClassifier& scheme_classifier);
  void StopAutocomplete();
  bool MatchSelected(uint32_t index,
                     const GURL& url,
                     GURL& destination_url,
                     ui::PageTransition& transition_type);
  void DidStartNavigation();

 private:
  RemoteNtpSearchProvider(const RemoteNtpSearchProvider&) = delete;
  RemoteNtpSearchProvider& operator=(const RemoteNtpSearchProvider&) = delete;

  // Overridden from AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  raw_ptr<Delegate> delegate_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;
  base::TimeTicks time_of_first_autocomplete_query_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SEARCH_PROVIDER_H_
