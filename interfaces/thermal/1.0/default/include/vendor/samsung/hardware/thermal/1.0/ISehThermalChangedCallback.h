/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_ISEHTHERMALCHANGEDCALLBACK_H
#define VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_ISEHTHERMALCHANGEDCALLBACK_H

#include <android/hidl/base/1.0/IBase.h>
#include <hidl/Status.h>
#if __has_include(<vendor/samsung/hardware/thermal/1.0/types.h>)
#include <vendor/samsung/hardware/thermal/1.0/types.h>
#else
#include "types.h"
#endif

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {

struct ISehThermalChangedCallback : public ::android::hidl::base::V1_0::IBase {
    virtual ::android::hardware::Return<void> notifyTemperatures(
            const ::android::hardware::hidl_vec<SehTemperature>& temperatures) = 0;
};

}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor

#endif  // VENDOR_SAMSUNG_HARDWARE_THERMAL_V1_0_ISEHTHERMALCHANGEDCALLBACK_H
