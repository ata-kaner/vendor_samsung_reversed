/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_ISEHTHERMAL_H
#define VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_ISEHTHERMAL_H

#include <android/hardware/thermal/1.0/types.h>
#include <android/hardware/thermal/2.0/IThermal.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#if __has_include(<vendor/samsung/hardware/thermal/1.0/ISehThermalChangedCallback.h>)
#include <vendor/samsung/hardware/thermal/1.0/ISehThermalChangedCallback.h>
#include <vendor/samsung/hardware/thermal/1.0/types.h>
#else
#include "ISehThermalChangedCallback.h"
#include "types.h"
#endif

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {

struct ISehThermal : public ::android::hardware::thermal::V2_0::IThermal {
    using sehGetTemperatures_cb = std::function<void(
            const ::android::hardware::thermal::V1_0::ThermalStatus& status,
            const ::android::hardware::hidl_vec<SehTemperature>& temperatures)>;
    using sehGetTypeTemperatures_cb = std::function<void(
            const ::android::hardware::thermal::V1_0::ThermalStatus& status,
            const ::android::hardware::hidl_vec<SehTemperature>& temperatures)>;
    using sehRegisterThermalChangedCallback_cb = std::function<void(
            const ::android::hardware::thermal::V1_0::ThermalStatus& status)>;
    using sehUnregisterThermalChangedCallback_cb = std::function<void(
            const ::android::hardware::thermal::V1_0::ThermalStatus& status)>;

    virtual ::android::hardware::Return<void> sehGetTemperatures(
            sehGetTemperatures_cb _hidl_cb) = 0;
    virtual ::android::hardware::Return<void> sehGetTypeTemperatures(
            SehTemperatureType type,
            sehGetTypeTemperatures_cb _hidl_cb) = 0;
    virtual ::android::hardware::Return<void> sehRegisterThermalChangedCallback(
            const ::android::sp<ISehThermalChangedCallback>& callback,
            sehRegisterThermalChangedCallback_cb _hidl_cb) = 0;
    virtual ::android::hardware::Return<void> sehUnregisterThermalChangedCallback(
            const ::android::sp<ISehThermalChangedCallback>& callback,
            sehUnregisterThermalChangedCallback_cb _hidl_cb) = 0;

    static const char* descriptor;
    static ::android::sp<ISehThermal> getService(const std::string& serviceName = "default", bool getStub = false);
    ::android::status_t registerAsService(const std::string& serviceName = "default");
};

}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor

#endif  // VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_ISEHTHERMAL_H
