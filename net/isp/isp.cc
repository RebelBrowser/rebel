#include "net/isp/isp.h"

#include <memory>
#include <mutex>

#include "base/check.h"
#include "base/no_destructor.h"

struct State {
  State() : isp_("unknown"), ip_("unknown") {}

  std::string isp_;
  std::string ip_;
  std::mutex mutex_;

  std::string GetISP() {
    std::lock_guard<std::mutex> lock(mutex_);
    return isp_;
  }

  std::string GetIP() {
    std::lock_guard<std::mutex> lock(mutex_);
    return ip_;
  }

  bool SetISP(const std::string& isp) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isp_ != isp) {
      isp_ = isp;
      return true;
    }
    return false;
  }

  bool SetIP(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ip_ != ip) {
      ip_ = ip;
      return true;
    }
    return false;
  }

  static base::NoDestructor<std::unique_ptr<State>> instance;

  static void Initialize() { *instance = std::make_unique<State>(); }
  static State* Get() {
    State* state = instance->get();
    DCHECK(state);
    return state;
  }
};

base::NoDestructor<std::unique_ptr<State>> State::instance(nullptr);

namespace net {

// static
std::string ISP::GetISP() {
  return State::Get()->GetISP();
}

// static
std::string ISP::GetIP() {
  return State::Get()->GetIP();
}

// static
bool ISP::SetISP(const std::string& isp) {
  return State::Get()->SetISP(isp);
}

// static
bool ISP::SetIP(const std::string& ip) {
  return State::Get()->SetIP(ip);
}

// static
void ISP::Initialize() {
  State::Initialize();
}

}  // namespace net