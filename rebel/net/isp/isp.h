// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REBEL_NET_ISP_ISP_H_
#define REBEL_NET_ISP_ISP_H_

#include <mutex>
#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"

#include "rebel/base/metrics/histogram_macros.h"

#define PAGE_LOAD_HISTOGRAM_DYNAMIC(name, sample)                          \
  UMA_HISTOGRAM_CUSTOM_TIMES_DYNAMIC(name, sample, base::Milliseconds(10), \
                                     base::Minutes(10), 100)

namespace rebel {

extern NET_EXPORT const char kEnablePerISPHistograms[];

extern NET_EXPORT const char kPerISPHistogramsName[];
extern NET_EXPORT const char kPerISPHistogramsDescription[];

class NET_EXPORT ISP {
 public:
  static ISP* Instance();

  [[nodiscard]] const std::string& GetISP() const;
  [[nodiscard]] const std::string& GetIP() const;

  bool SetISP(std::string isp);
  bool SetIP(std::string ip);

 private:
  mutable std::mutex mutex_;
  std::string isp_{"unknown"};
  std::string ip_{"unknown"};
};

NET_EXPORT std::string PerISPHistogramName(const char* name);

}  // namespace rebel

#endif  // REBEL_NET_ISP_ISP_H_
