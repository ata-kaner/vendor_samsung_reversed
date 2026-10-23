//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <signal.h>
#include <time.h>
#include <functional>

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

class IntervalTimer {
  public:
    IntervalTimer();
    ~IntervalTimer();

    void set_callback(std::function<void()> cb);
    bool start(int ms);
    const std::function<void()>& get_callback() const { return mCallback; }

  private:
    timer_t mTimerId;
    std::function<void()> mCallback;
};

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
