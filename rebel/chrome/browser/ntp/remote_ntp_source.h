// Copyright 2022 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SOURCE_H_
#define REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SOURCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace rebel {

// Data source to load chrome-search://remote-ntp-offline when the RemoteNTP
// failed to load.
class RemoteNtpSource : public content::URLDataSource {
 public:
  RemoteNtpSource(Profile* profile);
  ~RemoteNtpSource() override;

 private:
  RemoteNtpSource(const RemoteNtpSource&) = delete;
  RemoteNtpSource& operator=(const RemoteNtpSource&) = delete;

  // Overridden from content::URLDataSource:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool AllowCaching() override;
  bool ShouldAddContentSecurityPolicy() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;

  raw_ptr<Profile> profile_;
};

}  // namespace rebel

#endif  // REBEL_CHROME_BROWSER_NTP_REMOTE_NTP_SOURCE_H_
