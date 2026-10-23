/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "TemperatureProvider.h"

#include <android/log.h>
#include <cmath>

#define LOG_TAG "SehThermal-HAL"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_vec;
using ::android::hardware::thermal::V2_0::Temperature;
using ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;

TemperatureProvider* TemperatureProvider::sInstance = nullptr;

TemperatureProvider::TemperatureProvider(NotifyCallback notifyCb)
    : mNotifyCb(notifyCb),
      mIntervalMs(100) {
    sInstance = this;

    mBasicGroup = std::make_unique<BasicThermistorGroup>();
    mModemGroup = std::make_unique<QcModemThermistorGroup>();
    mVirtualGroup = std::make_unique<VirtualThermistorGroup>();
    mBootControl = std::make_unique<BootControl>();

    initGroups();

    // Perform initial reading
    update();

    // Arm initial 100ms timer
    mTimer.set(mIntervalMs, &TemperatureProvider::timerCallback);
}

TemperatureProvider::~TemperatureProvider() {
    mTimer.kill();
    sInstance = nullptr;
}

void TemperatureProvider::initGroups() {
    mTypes.clear();
    mBasicGroup->get_types(&mTypes);
    mModemGroup->get_types(&mTypes);
    mVirtualGroup->get_types(&mTypes);
}

void TemperatureProvider::timerCallback(sigval /* val */) {
    if (sInstance) {
        sInstance->onTimer();
    }
}

void TemperatureProvider::onTimer() {
    // If BootControl finishes, transition to steady-state polling interval (5000 ms)
    if (mBootControl && mBootControl->check()) {
        mIntervalMs = 5000;
    }

    // Re-arm interval timer
    mTimer.set(mIntervalMs, &TemperatureProvider::timerCallback);

    update();
}

void TemperatureProvider::update() {
    std::lock_guard<std::mutex> lock(mLock);

    std::map<std::string, int> updatedTemps;

    // 1. Read physical thermistors
    if (mBasicGroup) {
        mBasicGroup->get_temperatures(&updatedTemps);
    }

    // 2. Read Qualcomm modem thermal zones
    if (mModemGroup) {
        mModemGroup->get_temperatures(&updatedTemps);
    }

    // 3. Evaluate Mercury Linear Regression Predictor (LRP) for virtual skin sensors
    if (mVirtualGroup) {
        mVirtualGroup->get_temperatures(&updatedTemps);
    }

    mCurrentTemps = updatedTemps;

    // 4. Pass skin temperature to BootControl
    if (mBootControl) {
        int skinTemp = -300;
        auto itSkin = mCurrentTemps.find("SKIN");
        if (itSkin != mCurrentTemps.end()) {
            skinTemp = itSkin->second;
        } else {
            auto itLrp = mCurrentTemps.find("LRP");
            if (itLrp != mCurrentTemps.end()) {
                skinTemp = itLrp->second;
            }
        }

        if (skinTemp > -300) {
            mBootControl->control(skinTemp);
        }
    }

    // 5. Build HIDL temperature vectors and notify listeners
    if (mNotifyCb) {
        std::vector<SehTemperature> sehVec;
        std::vector<Temperature> v2Vec;

        for (const auto& pair : mCurrentTemps) {
            if (pair.second <= -300) {
                continue;
            }

            const std::string& name = pair.first;
            float floatVal = static_cast<float>(pair.second) / 10.0f;

            SehTemperatureType sehType = SehTemperatureType::SKIN;
            auto itType = mTypes.find(name);
            if (itType != mTypes.end()) {
                sehType = itType->second;
            }

            SehTemperature sehTemp;
            sehTemp.type = sehType;
            sehTemp.name = name;
            sehTemp.currentValue = floatVal;
            sehVec.push_back(sehTemp);

            Temperature v2Temp;
            v2Temp.type = static_cast<TemperatureType>(sehType);
            v2Temp.name = name;
            v2Temp.value = floatVal;
            v2Temp.throttlingStatus = ThrottlingSeverity::NONE;
            v2Vec.push_back(v2Temp);
        }

        hidl_vec<SehTemperature> sehHidl;
        sehHidl.setToExternal(sehVec.data(), sehVec.size());

        hidl_vec<Temperature> v2Hidl;
        v2Hidl.setToExternal(v2Vec.data(), v2Vec.size());

        mNotifyCb(sehHidl, v2Hidl);
    }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
