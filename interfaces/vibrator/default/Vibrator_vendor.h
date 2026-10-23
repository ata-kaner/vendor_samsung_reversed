/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "HwApi.h"

#include <aidl/vendor/samsung/hardware/vibrator/BnSehVibrator.h>

#include <memory>
#include <string>
#include <vector>

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace vibrator {

class Vibrator : public BnSehVibrator {
  public:
    Vibrator(std::shared_ptr<aidl::android::hardware::vibrator::HwApi> hwApi);
    virtual ~Vibrator() = default;

    ndk::ScopedAStatus supportsAmplitudeControl(bool* _aidl_return) override;
    ndk::ScopedAStatus supportsForceTouchAmplitudeControl(bool* _aidl_return) override;
    ndk::ScopedAStatus supportsFrequencyControl(bool* _aidl_return) override;
    ndk::ScopedAStatus supportsHapticEngine(bool* _aidl_return) override;
    ndk::ScopedAStatus supportsPrebakedHapticPattern(bool* _aidl_return) override;
    ndk::ScopedAStatus supportsFolderStatusSetting(bool* _aidl_return) override;
    ndk::ScopedAStatus supportsEnhancedSamsungHapticPattern(bool* _aidl_return) override;
    ndk::ScopedAStatus setAmplitude(int32_t amplitude) override;
    ndk::ScopedAStatus setForceTouchAmplitude(int32_t amplitude) override;
    ndk::ScopedAStatus setFrequencyType(SehFrequencyType type) override;
    ndk::ScopedAStatus setFolderStatus(SehFolderStatus status) override;
    ndk::ScopedAStatus getNumberOfPrebakedHapticPattern(int32_t* _aidl_return) override;
    ndk::ScopedAStatus performHapticEngine(const std::vector<SehHapticEnginePacket>& packets,
                                          int32_t repeat) override;
    ndk::ScopedAStatus performPrebakedHapticPattern(int32_t index, int32_t timeout,
                                                   bool repeat) override;
    ndk::ScopedAStatus getAmplitudeList(SehAmplitudeType type,
                                       std::vector<int32_t>* _aidl_return) override;
    ndk::ScopedAStatus perform(
            aidl::android::hardware::vibrator::Effect effect, int32_t strength,
            const std::shared_ptr<aidl::android::hardware::vibrator::IVibratorCallback>& callback,
            int32_t* _aidl_return) override;
    ndk::ScopedAStatus performPlaybackEngine(const std::vector<SehPlaybackPacket>& packets,
                                            bool repeat, int32_t timeout) override;
    ndk::ScopedAStatus supportsFifoHapticEngine(bool* _aidl_return) override;
    ndk::ScopedAStatus supportsHybridHapticEngine(bool* _aidl_return) override;
    ndk::ScopedAStatus supportsEnhancedHybridHapticPattern(bool* _aidl_return) override;
    ndk::ScopedAStatus performFifoHapticEngine(SehFifoType type) override;
    ndk::ScopedAStatus performHybridHapticEngine(
            const std::vector<SehHybridHapticEnginePacket>& packets, int32_t repeat) override;
    ndk::ScopedAStatus supportsFolderTentModeStatusSetting(bool* _aidl_return) override;
    ndk::ScopedAStatus setFolderTentModeStatus(SehFolderTentModeStatus status) override;
    ndk::ScopedAStatus supportsHasFeature(bool* _aidl_return) override;
    ndk::ScopedAStatus hasFeature(const std::string& feature, bool* _aidl_return) override;
    ndk::ScopedAStatus performCommonInputff(
            const std::vector<SehCommonInputffPacket>& packets, bool repeat,
            int32_t timeout) override;

  private:
    bool __getDynamicAmplitudeList(SehAmplitudeType type, std::vector<int32_t>* _aidl_return);

    std::shared_ptr<aidl::android::hardware::vibrator::HwApi> mHwApi;
};

}  // namespace vibrator
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
