/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEMPERATURE_PROVIDER_H
#define TEMPERATURE_PROVIDER_H

#include <android/hardware/thermal/2.0/IThermal.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "BootControl.h"
#include "IntervalTimer.h"
#include "Thermistors.h"
#if __has_include(<vendor/samsung/hardware/thermal/1.0/types.h>)
#include <vendor/samsung/hardware/thermal/1.0/types.h>
#else
#include "include/vendor/samsung/hardware/thermal/1.0/types.h"
#endif

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

class SehThermal;

class TemperatureProvider {
public:
    using NotifyCallback = std::function<void(
            ::android::hardware::hidl_vec<SehTemperature>& sehTemps,
            ::android::hardware::hidl_vec<::android::hardware::thermal::V2_0::Temperature>& temps_2_0)>;

    TemperatureProvider(NotifyCallback notifyCb);
    ~TemperatureProvider();

    void update();
    void onTimer();

    const std::map<std::string, int>& getTemperatures() const { return mCurrentTemps; }
    const std::map<std::string, SehTemperatureType>& getTypes() const { return mTypes; }

    static TemperatureProvider* getInstance() { return sInstance; }

private:
    void initGroups();
    static void timerCallback(sigval val);

    static TemperatureProvider* sInstance;

    NotifyCallback mNotifyCb;
    std::mutex mLock;

    int mIntervalMs;
    IntervalTimer mTimer;

    std::unique_ptr<BasicThermistorGroup> mBasicGroup;
    std::unique_ptr<QcModemThermistorGroup> mModemGroup;
    std::unique_ptr<VirtualThermistorGroup> mVirtualGroup;
    std::unique_ptr<BootControl> mBootControl;

    std::map<std::string, SehTemperatureType> mTypes;
    std::map<std::string, int> mCurrentTemps;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor

#endif  // TEMPERATURE_PROVIDER_H
