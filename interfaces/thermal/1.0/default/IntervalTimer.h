/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INTERVAL_TIMER_H
#define INTERVAL_TIMER_H

#include <signal.h>
#include <time.h>

class IntervalTimer {
public:
    IntervalTimer();
    ~IntervalTimer();

    bool set(int ms, void (*callback)(sigval));
    void kill();

private:
    timer_t mTimerId;
    void (*mCallback)(sigval);
};

#endif  // INTERVAL_TIMER_H
