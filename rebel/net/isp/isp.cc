// Copyright 2023 Viasat Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rebel/net/isp/isp.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/no_destructor.h"

namespace rebel {

const char kEnablePerISPHistograms[] = "enable-per-isp-histograms";

const char kPerISPHistogramsName[] = "Per ISP network latency histogram";
const char kPerISPHistogramsDescription[] =
    "Collect network latency samples into separate histograms based on the "
    "ISP.";

// static
ISP* ISP::Instance() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEnablePerISPHistograms)) {
    return nullptr;
  }

  static base::NoDestructor<ISP> instance;
  return instance.get();
}

// static
const std::string& ISP::GetISP() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return isp_;
}

// static
const std::string& ISP::GetIP() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return ip_;
}

// static
bool ISP::SetISP(std::string isp) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (isp_ == isp) {
    return false;
  }

  isp_ = std::move(isp);
  return true;
}

// static
bool ISP::SetIP(std::string ip) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (ip_ == ip) {
    return false;
  }

  ip_ = std::move(ip);
  return true;
}

std::string PerISPHistogramName(const char* name) {
  if (auto* isp = ISP::Instance()) {
    return std::string(name) + " (isp = " + isp->GetISP() + ")";
  }

  return name;
}

}  // namespace rebel
