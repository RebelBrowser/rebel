#ifndef BASE_METRICS_PER_ISP_HISTOGRAM_H_
#define BASE_METRICS_PER_ISP_HISTOGRAM_H_

#include <string>

#include "base/command_line.h"
#include "net/isp/isp.h"

#define PER_ISP_HISTOGRAM(name)                                         \
  []() {                                                                \
    static bool enable_per_isp_histograms =                             \
        base::CommandLine::ForCurrentProcess()->HasSwitch(              \
            "enable-per-isp-histograms");                               \
    if (enable_per_isp_histograms) {                                    \
      return std::string(name) + " (isp = " + net::ISP::GetISP() + ")"; \
    }                                                                   \
    return std::string(name);                                           \
  }()

#endif  // BASE_METRICS_PER_ISP_HISTOGRAM_H_
