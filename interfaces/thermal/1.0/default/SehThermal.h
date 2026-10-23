/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SEH_THERMAL_H
#define SEH_THERMAL_H

#include <android/hardware/thermal/1.0/IThermal.h>
#include <android/hardware/thermal/2.0/IThermal.h>
#include <android/hardware/thermal/2.0/IThermalChangedCallback.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <mutex>
#include <vector>

#if __has_include(<vendor/samsung/hardware/thermal/1.0/ISehThermal.h>)
#include <vendor/samsung/hardware/thermal/1.0/ISehThermal.h>
#include <vendor/samsung/hardware/thermal/1.0/ISehThermalChangedCallback.h>
#include <vendor/samsung/hardware/thermal/1.0/types.h>
#else
#include "include/vendor/samsung/hardware/thermal/1.0/ISehThermal.h"
#include "include/vendor/samsung/hardware/thermal/1.0/ISehThermalChangedCallback.h"
#include "include/vendor/samsung/hardware/thermal/1.0/types.h"
#endif

#include "TemperatureProvider.h"

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

using ::android::sp;
using ::android::wp;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hidl::base::V1_0::IBase;

using ::android::hardware::thermal::V1_0::CpuUsage;
using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;
using CoolingDevice_1_0 = ::android::hardware::thermal::V1_0::CoolingDevice;
using Temperature_1_0 = ::android::hardware::thermal::V1_0::Temperature;

using ::android::hardware::thermal::V2_0::CoolingType;
using ::android::hardware::thermal::V2_0::IThermalChangedCallback;
using ::android::hardware::thermal::V2_0::TemperatureThreshold;
using ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;
using CoolingDevice_2_0 = ::android::hardware::thermal::V2_0::CoolingDevice;
using Temperature_2_0 = ::android::hardware::thermal::V2_0::Temperature;

struct CallbackSetting {
    CallbackSetting(sp<IThermalChangedCallback> cb, bool filter, TemperatureType t)
        : callback(cb), filterType(filter), type(t) {}
    sp<IThermalChangedCallback> callback;
    bool filterType;
    TemperatureType type;
};

class SehThermal : public ISehThermal, public hidl_death_recipient {
public:
    SehThermal();
    ~SehThermal() override;

    // Methods from ::android::hardware::thermal::V1_0::IThermal
    Return<void> getTemperatures(getTemperatures_cb _hidl_cb) override;
    Return<void> getCpuUsages(getCpuUsages_cb _hidl_cb) override;
    Return<void> getCoolingDevices(getCoolingDevices_cb _hidl_cb) override;

    // Methods from ::android::hardware::thermal::V2_0::IThermal
    Return<void> getCurrentTemperatures(bool filterType, TemperatureType type,
                                        getCurrentTemperatures_cb _hidl_cb) override;
    Return<void> getTemperatureThresholds(bool filterType, TemperatureType type,
                                          getTemperatureThresholds_cb _hidl_cb) override;
    Return<void> registerThermalChangedCallback(
            const sp<IThermalChangedCallback>& callback, bool filterType, TemperatureType type,
            registerThermalChangedCallback_cb _hidl_cb) override;
    Return<void> unregisterThermalChangedCallback(
            const sp<IThermalChangedCallback>& callback,
            unregisterThermalChangedCallback_cb _hidl_cb) override;
    Return<void> getCurrentCoolingDevices(bool filterType, CoolingType type,
                                          getCurrentCoolingDevices_cb _hidl_cb) override;

    // Methods from ::vendor::samsung::hardware::thermal::V1_0::ISehThermal
    Return<void> sehGetTemperatures(sehGetTemperatures_cb _hidl_cb) override;
    Return<void> sehGetTypeTemperatures(SehTemperatureType type,
                                        sehGetTypeTemperatures_cb _hidl_cb) override;
    Return<void> sehRegisterThermalChangedCallback(
            const sp<ISehThermalChangedCallback>& callback,
            sehRegisterThermalChangedCallback_cb _hidl_cb) override;
    Return<void> sehUnregisterThermalChangedCallback(
            const sp<ISehThermalChangedCallback>& callback,
            sehUnregisterThermalChangedCallback_cb _hidl_cb) override;

    // Death recipient callback
    void serviceDied(uint64_t cookie, const wp<IBase>& who) override;

    // Listener dispatch from TemperatureProvider
    void notifyToListeners(hidl_vec<SehTemperature>& sehTemps,
                           hidl_vec<Temperature_2_0>& v2Temps);

private:
    ThrottlingSeverity getThrottlingSeverity(TemperatureType type, float value,
                                            const std::string& name);

    std::mutex mCallbackLock;
    std::vector<CallbackSetting> mCallbacks;
    std::vector<sp<ISehThermalChangedCallback>> mSehCallbacks;

    std::unique_ptr<TemperatureProvider> mTemperatureProvider;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor

#endif  // SEH_THERMAL_H
