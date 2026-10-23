//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include "IntervalTimer.h"

#include <android/log.h>

void interval_timer_callback(sigval val) {
    auto timer = reinterpret_cast<vendor::samsung::hardware::thermal::IntervalTimer*>(val.sival_ptr);
    if (timer && timer->get_callback()) {
        timer->get_callback()();
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "thermal_timer", "timer callback not registered");
    }
}

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

IntervalTimer::IntervalTimer() : mTimerId(0) {}

IntervalTimer::~IntervalTimer() {
    if (mTimerId != 0) {
        timer_delete(mTimerId);
        mTimerId = 0;
    }
}

void IntervalTimer::set_callback(std::function<void()> cb) {
    mCallback = std::move(cb);
}

bool IntervalTimer::start(int ms) {
    if (mTimerId == 0) {
        if (!mCallback) {
            return false;
        }
        sigevent se{};
        se.sigev_notify = SIGEV_THREAD;
        se.sigev_value.sival_ptr = this;
        se.sigev_notify_function = interval_timer_callback;
        if (timer_create(CLOCK_MONOTONIC, &se, &mTimerId) == -1) {
            __android_log_print(ANDROID_LOG_DEBUG, "thermal_timer", "fail create timer");
            return false;
        }
    }

    itimerspec its{};
    its.it_value.tv_sec = ms / 1000;
    its.it_value.tv_nsec = (ms % 1000) * 1000000LL;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(mTimerId, 0, &its, nullptr) == -1) {
        __android_log_print(ANDROID_LOG_DEBUG, "thermal_timer", "fail set timer");
        return false;
    }
    return true;
}

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
