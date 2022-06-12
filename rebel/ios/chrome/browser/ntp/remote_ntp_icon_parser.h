// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_PARSER_H_
#define REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_PARSER_H_

#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class ScriptMessage;
class WebState;

}  // namespace web

namespace rebel {

// WebState message handler to receive icons of the following form:
//
//     <link rel="icon" ...>
//     <link rel="shortcut icon" ...>
//     <link rel="fluid-icon" ...>
//     <link rel="apple-touch-icon" ...>
//     <link rel="apple-touch-icon-precomposed" ...>
//
// The largest icon is sent to the RemoteNtpService for fetching and storage.
class RemoteNtpIconParser : public web::JavaScriptFeature {
 public:
  RemoteNtpIconParser();
  ~RemoteNtpIconParser() override;

 private:
  RemoteNtpIconParser(const RemoteNtpIconParser&) = delete;
  RemoteNtpIconParser& operator=(const RemoteNtpIconParser&) = delete;

  // web:JavaScriptFeature:
  absl::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;
};

}  // namespace rebel

#endif  // REBEL_IOS_CHROME_BROWSER_NTP_REMOTE_NTP_ICON_PARSER_H_
