// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/chrome/renderer/ntp/remote_ntp_extension.h"

#include <string>

#include "base/system/sys_info.h"
#include "components/version_info/version_info.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8.h"

#include "rebel/chrome/common/ntp/remote_ntp_types.h"
#include "rebel/chrome/renderer/ntp/remote_ntp.h"
#include "rebel/components/url_formatter/rebel_constants.h"

namespace rebel {

namespace {

constexpr const char kDispatchNtpTilesChangedScript[] =
    "if (window.rebel &&"
    "    window.rebel.onNtpTilesChanged &&"
    "    (typeof window.rebel.onNtpTilesChanged === 'function')) {"
    "  window.rebel.onNtpTilesChanged();"
    "  true;"
    "}";

constexpr const char kDispatchAutocompleteResultChanged[] =
    "if (window.rebel &&"
    "    window.rebel.search &&"
    "    window.rebel.search.onAutocompleteResultChanged &&"
    "    (typeof window.rebel.search.onAutocompleteResultChanged"
    "        ===  'function')) {"
    "  window.rebel.search.onAutocompleteResultChanged();"
    "  true;"
    "}";

constexpr const char kDispatchThemeChangedScript[] =
    "if (window.rebel &&"
    "    window.rebel.theme &&"
    "    window.rebel.theme.onThemeChanged &&"
    "    (typeof window.rebel.theme.onThemeChanged === 'function')) {"
    "  window.rebel.theme.onThemeChanged();"
    "  true;"
    "}";

constexpr const char kDispatchWiFiStatusChangedScript[] =
    "if (window.rebel &&"
    "    window.rebel.network &&"
    "    window.rebel.network.onWiFiStatusChanged &&"
    "    (typeof window.rebel.network.onWiFiStatusChanged === 'function')) {"
    "  window.rebel.network.onWiFiStatusChanged();"
    "  true;"
    "}";

void Dispatch(blink::WebLocalFrame* frame, const blink::WebString& script) {
  if (frame) {
    frame->ExecuteScript(blink::WebScriptSource(script));
  }
}

// Populates a Javascript NTP tile object for returning from rebel.ntpTiles.
// TODO: For security, tiles should be in an <iframe>.
v8::Local<v8::Object> GenerateNtpTile(
    v8::Isolate* isolate,
    RemoteNtp::RemoteNtpTileCache::ItemIDPair id_and_tile) {
  return gin::DataObjectBuilder(isolate)
      .Set("title", id_and_tile.second.title)
      .Set("url", id_and_tile.second.url)
      .Set("favicon_url", id_and_tile.second.favicon_url)
      .Build();
}

// Populates a Javascript autocomplete classification object for returning from
// rebel.search.autocompleteResult.matches.(contentsClass|descriptionClass).
v8::Local<v8::Object> GenerateMatchClassification(
    v8::Isolate* isolate,
    const std::vector<rebel::mojom::ACMatchClassificationPtr>&
        classifications) {
  v8::Local<v8::Object> v8_result =
      v8::Array::New(isolate, classifications.size());
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for (size_t i = 0; i < classifications.size(); ++i) {
    v8::Local<v8::Object> v8_entry =
        gin::DataObjectBuilder(isolate)
            .Set("offset", classifications[i]->offset)
            .Set("style", classifications[i]->style)
            .Build();

    v8_result->CreateDataProperty(context, i, v8_entry).Check();
  }

  return v8_result;
}

// Populates a Javascript autocomplete result object for returning from
// rebel.search.autocompleteResult.
v8::Local<v8::Object> GenerateAutocompleteMatches(
    v8::Isolate* isolate,
    const std::vector<rebel::mojom::AutocompleteMatchPtr>& matches) {
  v8::Local<v8::Object> v8_matches = v8::Array::New(isolate, matches.size());
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for (size_t i = 0; i < matches.size(); ++i) {
    const rebel::mojom::AutocompleteMatchPtr& match = matches[i];

    v8::Local<v8::Object> v8_match =
        gin::DataObjectBuilder(isolate)
            .Set("contents", match->contents)
            .Set("contentsClass",
                 GenerateMatchClassification(isolate, match->contents_class))
            .Set("description", match->description)
            .Set("descriptionClass",
                 GenerateMatchClassification(isolate, match->description_class))
            .Set("destinationUrl", match->destination_url)
            .Set("type", match->type)
            .Set("isSearchType", match->is_search_type)
            .Set("fillIntoEdit", match->fill_into_edit)
            .Set("inlineAutocompletion", match->inline_autocompletion)
            .Set("allowedToBeDefaultMatch", match->allowed_to_be_default_match)
            .Build();

    v8_matches->CreateDataProperty(context, i, v8_match).Check();
  }

  return v8_matches;
}

// Populates a Javascript theme object for returning from rebel.theme.theme.
v8::Local<v8::Object> GenerateTheme(
    v8::Isolate* isolate,
    const rebel::mojom::RemoteNtpThemePtr& theme) {
  v8::Local<v8::Object> background =
      gin::DataObjectBuilder(isolate)
          .Set("imageUrl", theme->image_url.spec())
          .Set("imageAlignment", theme->image_alignment)
          .Set("imageTiling", theme->image_tiling)
          .Set("attributionLine1", theme->attribution_line_1)
          .Set("attributionLine2", theme->attribution_line_2)
          .Set("attributionUrl", theme->attribution_url.spec())
          .Set("attributionImageUrl", theme->attribution_image_url.spec())
          .Build();

  return gin::DataObjectBuilder(isolate)
      .Set("darkModeEnabled", theme->dark_mode_enabled)
      .Set("background", background)
      .Build();
}

// Populates a Javascript theme object for returning from
// rebel.network.wiFiStatus.
v8::Local<v8::Object> GenerateWiFiStatus(
    v8::Isolate* isolate,
    const rebel::RemoteNtpWiFiStatusList& wifi_status) {
  v8::Local<v8::Object> v8_wifi_status =
      v8::Array::New(isolate, wifi_status.size());
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  for (size_t i = 0; i < wifi_status.size(); ++i) {
    const auto& status = wifi_status[i];

    v8::Local<v8::Object> v8_status =
        gin::DataObjectBuilder(isolate)
            .Set("ssid", status->ssid)
            .Set("bssid", status->bssid)
            .Set("connectionState", status->connection_state)
            .Set("rssi", status->rssi)
            .Set("signalLevel", status->signal_level)
            .Set("maxSignalLevel", status->max_signal_level)
            .Set("frequency", status->frequency)
            .Set("linkSpeed", status->link_speed)
            .Set("rxMbps", status->rx_mbps)
            .Set("txMbps", status->tx_mbps)
            .Set("maxRxMbps", status->max_rx_mbps)
            .Set("maxTxMbps", status->max_tx_mbps)
            .Set("noiseMeasurement", status->noise_measurement)
            .Build();

    v8_wifi_status->CreateDataProperty(context, i, v8_status).Check();
  }

  return v8_wifi_status;
}

bool IsUrlValid(const GURL& url) {
  return url.is_valid() && !blink::IsRendererDebugURL(url);
}

content::RenderFrame* GetMainRenderFrameForCurrentContext() {
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForCurrentContext();
  if (!frame) {
    return nullptr;
  }

  content::RenderFrame* main_frame =
      content::RenderFrame::FromWebFrame(frame->LocalRoot());
  if (!main_frame || !main_frame->IsMainFrame()) {
    return nullptr;
  }

  return main_frame;
}

RemoteNtp* GetRemoteNtpForCurrentContext() {
  content::RenderFrame* main_frame = GetMainRenderFrameForCurrentContext();
  if (!main_frame) {
    return nullptr;
  }

  return RemoteNtp::Get(main_frame);
}

// Javascript to C++ bindings for window.rebel.
class RemoteNtpBindings : public gin::Wrappable<RemoteNtpBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  RemoteNtpBindings() {}
  ~RemoteNtpBindings() override {}

 private:
  RemoteNtpBindings(const RemoteNtpBindings&) = delete;
  RemoteNtpBindings& operator=(const RemoteNtpBindings&) = delete;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final {
    return gin::Wrappable<RemoteNtpBindings>::GetObjectTemplateBuilder(isolate)
        .SetProperty("ntpTilesAvailable",
                     &RemoteNtpBindings::AreNtpTilesAvailable)
        .SetProperty("ntpTiles", &RemoteNtpBindings::GetNtpTiles)
        .SetProperty("platformInfo", &RemoteNtpBindings::GetPlatformInfo)
        .SetMethod("addCustomTile", &RemoteNtpBindings::AddCustomTile)
        .SetMethod("removeCustomTile", &RemoteNtpBindings::RemoveCustomTile)
        .SetMethod("editCustomTile", &RemoteNtpBindings::EditCustomTile)
        .SetMethod("loadInternalUrl", &RemoteNtpBindings::LoadInternalUrl);
  }

  static bool AreNtpTilesAvailable() {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return false;
    }

    return remote_ntp->AreNtpTilesAvailable();
  }

  static v8::Local<v8::Value> GetNtpTiles(v8::Isolate* isolate) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return v8::Null(isolate);
    }

    RemoteNtp::RemoteNtpTileCache::ItemIDVector tiles;
    remote_ntp->GetNtpTiles(&tiles);

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Object> v8_tiles = v8::Array::New(isolate, tiles.size());

    for (size_t i = 0; i < tiles.size(); ++i) {
      v8::Local<v8::Object> tile = GenerateNtpTile(isolate, tiles[i]);
      v8_tiles->CreateDataProperty(context, i, tile).Check();
    }

    return v8_tiles;
  }

  static v8::Local<v8::Value> GetPlatformInfo(v8::Isolate* isolate) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return v8::Null(isolate);
    }

    static constexpr base::StringPiece platform = version_info::GetOSType();
    static constexpr base::StringPiece version =
        version_info::GetVersionNumber();

    // This is how chrome://version determines browser architecture.
    int browser_arch = (sizeof(void*) == 8) ? 64 : 32;

    const std::string arch_name = base::SysInfo::OperatingSystemArchitecture();
    int system_arch = -1;

    if (!arch_name.empty()) {
      system_arch = arch_name.find("64") != std::string::npos ? 64 : 32;
    }

    return gin::DataObjectBuilder(isolate)
        .Set("platform", platform)
        .Set("version", version)
        .Set("browserArch", browser_arch)
        .Set("systemArch", system_arch)
        .Build();
  }

  static void AddCustomTile(const std::string& tile_url,
                            const std::u16string& tile_title) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return;
    }

    const GURL validated_url(tile_url);

    if (!IsUrlValid(validated_url) || tile_title.empty()) {
      return;
    }

    remote_ntp->AddCustomTile(validated_url, tile_title);
  }

  static void RemoveCustomTile(const std::string& tile_url) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return;
    }

    const GURL validated_url(tile_url);

    if (!IsUrlValid(validated_url)) {
      return;
    }

    remote_ntp->RemoveCustomTile(validated_url);
  }

  static void EditCustomTile(const std::string& old_tile_url,
                             const std::string& new_tile_url,
                             const std::u16string& new_tile_title) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return;
    }

    const GURL old_validated_url(old_tile_url);
    const GURL new_validated_url(new_tile_url);

    if ((new_tile_url.empty() && new_tile_title.empty()) ||
        !IsUrlValid(old_validated_url) ||
        (!new_tile_url.empty() && !IsUrlValid(new_validated_url))) {
      return;
    }

    remote_ntp->EditCustomTile(old_validated_url, new_validated_url,
                               new_tile_title);
  }

  static bool LoadInternalUrl(const std::string& url) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return false;
    }

    const GURL validated_url(url);

    // Only allow chrome:// URLs that won't crash the browser.
    if (!IsUrlValid(validated_url) ||
        !rebel::SchemeIsRebelOrChrome(validated_url)) {
      return false;
    }

    remote_ntp->LoadInternalUrl(validated_url);
    return true;
  }
};

// Javascript to C++ bindings for window.rebel.search.
class SearchBindings : public gin::Wrappable<SearchBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  SearchBindings() {}
  ~SearchBindings() override {}

 private:
  SearchBindings(const SearchBindings&) = delete;
  SearchBindings& operator=(const SearchBindings&) = delete;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final {
    return gin::Wrappable<SearchBindings>::GetObjectTemplateBuilder(isolate)
        .SetProperty("autocompleteResult",
                     &SearchBindings::GetAutocompleteResult)
        .SetMethod("queryAutocomplete", &SearchBindings::QueryAutocomplete)
        .SetMethod("stopAutocomplete", &SearchBindings::StopAutocomplete)
        .SetMethod("openAutocompleteMatch",
                   &SearchBindings::OpenAutocompleteMatch);
  }

  static v8::Local<v8::Value> GetAutocompleteResult(v8::Isolate* isolate) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return v8::Null(isolate);
    }

    const rebel::mojom::AutocompleteResultPtr& result =
        remote_ntp->GetAutocompleteResult();
    if (!result) {
      return v8::Null(isolate);
    }

    return gin::DataObjectBuilder(isolate)
        .Set("input", result->input)
        .Set("matches", GenerateAutocompleteMatches(isolate, result->matches))
        .Build();
  }

  static void QueryAutocomplete(const std::u16string& input,
                                bool prevent_inline_autocomplete) {
    RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return;
    }

    remote_ntp->QueryAutocomplete(input, prevent_inline_autocomplete);
  }

  static void StopAutocomplete() {
    RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return;
    }

    remote_ntp->StopAutocomplete();
  }

  static void OpenAutocompleteMatch(uint32_t index,
                                    const std::string& url,
                                    bool middle_button,
                                    bool alt_key,
                                    bool ctrl_key,
                                    bool meta_key,
                                    bool shift_key) {
    RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return;
    }

    remote_ntp->OpenAutocompleteMatch(index, GURL(url), middle_button, alt_key,
                                      ctrl_key, meta_key, shift_key);
  }
};

// Javascript to C++ bindings for window.rebel.theme.
class ThemeBindings : public gin::Wrappable<ThemeBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  ThemeBindings() {}
  ~ThemeBindings() override {}

 private:
  ThemeBindings(const ThemeBindings&) = delete;
  ThemeBindings& operator=(const ThemeBindings&) = delete;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final {
    return gin::Wrappable<ThemeBindings>::GetObjectTemplateBuilder(isolate)
        .SetProperty("theme", &ThemeBindings::GetTheme)
        .SetMethod("showOrHideCustomizeMenu",
                   &ThemeBindings::ShowOrHideCustomizeMenu);
  }

  static v8::Local<v8::Value> GetTheme(v8::Isolate* isolate) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return v8::Null(isolate);
    }

    const rebel::mojom::RemoteNtpThemePtr& theme = remote_ntp->GetTheme();
    if (!theme) {
      return v8::Null(isolate);
    }

    return GenerateTheme(isolate, theme);
  }

  static void ShowOrHideCustomizeMenu() {
    RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return;
    }

    remote_ntp->ShowOrHideCustomizeMenu();
  }
};

// Javascript to C++ bindings for window.rebel.network.
class NetworkBindings : public gin::Wrappable<NetworkBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  NetworkBindings() {}
  ~NetworkBindings() override {}

 private:
  NetworkBindings(const NetworkBindings&) = delete;
  NetworkBindings& operator=(const NetworkBindings&) = delete;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final {
    return gin::Wrappable<NetworkBindings>::GetObjectTemplateBuilder(isolate)
        .SetProperty("wiFiStatus", &NetworkBindings::GetWiFiStatus)
        .SetMethod("updateWiFiStatus", &NetworkBindings::UpdateWiFiStatus);
  }

  static v8::Local<v8::Value> GetWiFiStatus(v8::Isolate* isolate) {
    const RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return v8::Null(isolate);
    }

    const rebel::RemoteNtpWiFiStatusList& status = remote_ntp->GetWiFiStatus();
    if (status.empty()) {
      return v8::Null(isolate);
    }

    return GenerateWiFiStatus(isolate, status);
  }

  static void UpdateWiFiStatus() {
    RemoteNtp* remote_ntp = GetRemoteNtpForCurrentContext();
    if (!remote_ntp) {
      return;
    }

    remote_ntp->UpdateWiFiStatus();
  }
};

gin::WrapperInfo RemoteNtpBindings::kWrapperInfo = {gin::kEmbedderNativeGin};
gin::WrapperInfo SearchBindings::kWrapperInfo = {gin::kEmbedderNativeGin};
gin::WrapperInfo ThemeBindings::kWrapperInfo = {gin::kEmbedderNativeGin};
gin::WrapperInfo NetworkBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace

// static
void RemoteNtpExtension::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  v8::Context::Scope context_scope(context);

  gin::Handle<RemoteNtpBindings> remote_ntp_controller =
      gin::CreateHandle(isolate, new RemoteNtpBindings());
  if (remote_ntp_controller.IsEmpty()) {
    return;
  }

  gin::Handle<SearchBindings> search_controller =
      gin::CreateHandle(isolate, new SearchBindings());
  if (search_controller.IsEmpty()) {
    return;
  }

  gin::Handle<ThemeBindings> theme_controller =
      gin::CreateHandle(isolate, new ThemeBindings());
  if (theme_controller.IsEmpty()) {
    return;
  }

  gin::Handle<NetworkBindings> network_controller =
      gin::CreateHandle(isolate, new NetworkBindings());
  if (network_controller.IsEmpty()) {
    return;
  }

  v8::Local<v8::Object> remote_ntp =
      remote_ntp_controller.ToV8()->ToObject(context).ToLocalChecked();
  remote_ntp
      ->Set(context, gin::StringToV8(isolate, "search"),
            search_controller.ToV8())
      .ToChecked();
  remote_ntp
      ->Set(context, gin::StringToV8(isolate, "theme"), theme_controller.ToV8())
      .ToChecked();
  remote_ntp
      ->Set(context, gin::StringToV8(isolate, "network"),
            network_controller.ToV8())
      .ToChecked();

  v8::Local<v8::Object> global = context->Global();
  global->Set(context, gin::StringToV8(isolate, "rebel"), remote_ntp)
      .ToChecked();
}

// static
void RemoteNtpExtension::DispatchNtpTilesChanged(blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchNtpTilesChangedScript);
}

// static
void RemoteNtpExtension::DispatchAutocompleteResultChanged(
    blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchAutocompleteResultChanged);
}

// static
void RemoteNtpExtension::DispatchThemeChanged(blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchThemeChangedScript);
}

// static
void RemoteNtpExtension::DispatchWiFiStatusChanged(
    blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchWiFiStatusChangedScript);
}

}  // namespace rebel
