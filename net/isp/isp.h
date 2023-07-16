#ifndef NET_ISP_ISP_H_
#define NET_ISP_ISP_H_

#include "net/base/net_export.h"

#include <string>

namespace net {

class NET_EXPORT ISP {
 public:
  [[nodiscard]] static std::string GetISP();

  [[nodiscard]] static std::string GetIP();

  static bool SetISP(const std::string& isp);

  static bool SetIP(const std::string& ip);

  static void Initialize();
};

}  // namespace net

#endif  // NET_ISP_ISP_H_
