/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "VibratorManager.h"

#include <android-base/logging.h>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

VibratorManager::VibratorManager(std::shared_ptr<IVibrator> vibrator)
    : mDefaultVibrator(std::move(vibrator)) {}

ndk::ScopedAStatus VibratorManager::getCapabilities(int32_t* _aidl_return) {
    LOG(VERBOSE) << "SecVibrator-HAL-AIDL-MANAGER getCapabilities";
    *_aidl_return = 0;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus VibratorManager::getVibratorIds(std::vector<int32_t>* _aidl_return) {
    LOG(VERBOSE) << "SecVibrator-HAL-AIDL-MANAGER getVibratorIds";
    *_aidl_return = {0};
    LOG(DEBUG) << "SecVibrator-HAL-AIDL-MANAGER getVibratorIds size: " << _aidl_return->size();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus VibratorManager::getVibrator(int32_t vibratorId,
                                               std::shared_ptr<IVibrator>* _aidl_return) {
    LOG(DEBUG) << "SecVibrator-HAL-AIDL-MANAGER getVibrator id: " << vibratorId;
    if (vibratorId == 0) {
        *_aidl_return = mDefaultVibrator;
        return ndk::ScopedAStatus::ok();
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus VibratorManager::prepareSynced(const std::vector<int32_t>& /*vibratorIds*/) {
    LOG(DEBUG) << "SecVibrator-HAL-AIDL-MANAGER prepareSynced unsupported";
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus VibratorManager::triggerSynced(
        const std::shared_ptr<IVibratorCallback>& /*callback*/) {
    LOG(DEBUG) << "SecVibrator-HAL-AIDL-MANAGER triggerSynced unsupported";
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus VibratorManager::cancelSynced() {
    LOG(DEBUG) << "SecVibrator-HAL-AIDL-MANAGER cancelSynced unsupported";
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
