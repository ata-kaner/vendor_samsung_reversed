/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

enum sec_features {
    INTENSITY = 0,
    FORCE_TOUCH_INTENSITY = 1,
    MULTI_FREQUENCY = 2,
    HAPTIC_ENGINE = 3,
    INDEX_HAPTIC_PATTERN = 4,
    SEPINDEX_HAPTIC_PATTERN = 5,
    ENHANCED_HAPTIC_ENGINE = 6,
    FIFO_HAPTIC_ENGINE = 7,
    HYBRID_HAPTIC_ENGINE = 8,
    ENHANCED_HYBRID_HAPTIC_ENGINE = 9,
    CAPABILITY_EXTERNAL_CONTROL = 10,
    COMMON_INPUTFF_INTERFACE = 11,
    PRIMITIVE_EFFECT_COMPOSE = 12,
};

enum sec_motor_types {
    MOTOR_NONE = 0,
    MOTOR_LINEAR_INDEX = 1,
    MOTOR_COINDC_INDEX = 2,
    MOTOR_COINDC_PMIC = 3,
    MOTOR_LINEAR_0832RC = 4,
    MOTOR_LINEAR_1040 = 5,
    MOTOR_LINEAR_RCVMOT = 6,
    MOTOR_LINEAR_1030 = 7,
    MOTOR_LINEAR_INDEX_RC = 8,
    MOTOR_LINEAR_WATCH = 9,
    MOTOR_LINEAR_ACTIVE = 10,
};

constexpr int32_t INTENSITY_MIN = 1;
constexpr int32_t INTENSITY_MAX = 10000;
constexpr int32_t INTENSITY_DEFAULT = 10000;

constexpr int32_t COMPOSITION_DELAY_MAX_MS = 1000;
constexpr int32_t COMPOSITION_SIZE_MAX = 10;

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
