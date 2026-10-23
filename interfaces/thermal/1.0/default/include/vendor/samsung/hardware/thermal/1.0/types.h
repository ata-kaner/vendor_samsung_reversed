/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_TYPES_H
#define VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_TYPES_H

#include <hidl/HidlSupport.h>
#include <cstdint>

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {

enum class SehTemperatureType : int32_t {
    CPU = 0,
    GPU = 1,
    BATTERY = 2,
    SKIN = 3,
    USB_PORT = 4,
    POWER_AMPLIFIER = 5,
    BCL_VOLTAGE = 6,
    BCL_CURRENT = 7,
    BCL_PERCENTAGE = 8,
    NPU = 9,
    SEH_MODEM = 50,
    SEH_CHARGER = 51,
    SEH_WIFI = 52,
    SEH_FLASH = 53,
    SEH_BACK = 54,
};

struct SehTemperature {
    SehTemperatureType type;
    ::android::hardware::hidl_string name;
    float currentValue;
};

}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor

#endif  // VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_TYPES_H
