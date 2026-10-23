/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "IntervalTimer.h"

#include <android/log.h>
#include <cstring>

#define LOG_TAG "SehThermal-HAL"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

IntervalTimer::IntervalTimer() : mTimerId(0), mCallback(nullptr) {}

IntervalTimer::~IntervalTimer() {
    kill();
}

bool IntervalTimer::set(int ms, void (*callback)(sigval)) {
    if (mCallback != callback || !mTimerId) {
        if (mTimerId) {
            timer_delete(mTimerId);
            mTimerId = 0;
        }

        struct sigevent sev{};
        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_notify_function = callback;
        sev.sigev_value.sival_ptr = this;

        if (timer_create(CLOCK_MONOTONIC, &sev, &mTimerId) == -1) {
            LOGE("fail create timer");
            return false;
        }
        mCallback = callback;
    }

    struct itimerspec its{};
    its.it_value.tv_sec = ms / 1000;
    its.it_value.tv_nsec = (ms % 1000) * 1000000;

    if (timer_settime(mTimerId, 0, &its, nullptr) == -1) {
        LOGE("fail set timer");
        return false;
    }

    return true;
}

void IntervalTimer::kill() {
    if (mTimerId) {
        timer_delete(mTimerId);
        mTimerId = 0;
        mCallback = nullptr;
    }
}
