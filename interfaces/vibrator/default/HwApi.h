/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "types.h"

#include <aidl/android/hardware/vibrator/Effect.h>
#include <aidl/android/hardware/vibrator/EffectStrength.h>
#include <linux/input.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

class HwApi {
  public:
    HwApi();
    virtual ~HwApi();

    bool checkClientUID(int uid);

    bool do_on(int timeout);
    bool do_off();

    int getAspEnable();
    bool hasAspEnable();
    bool setAspEnable(int enable);

    int getMotorIndex();
    int getMotorIndexbyMethod();
    int getMotorPerformMethod();

    bool hasFeature(sec_features feature);
    bool hasFeature(const std::string& feature);
    bool supportsHasFeature();

    bool isAmplitudeControlExist();
    bool isCalDynamicIntensitiesNodeExist();
    bool isHapticEngineNodeExist();
    bool isMotorTypeNodeExist();
    bool isNumWavesNodeExist();
    bool isPrebakedHapticPatternExist();
    bool isVibrating();
    bool isVibratorActiveNodeExist();
    bool isVibratorDurationNodeExist();
    bool isVibratorNodeExist();
    bool isVibratorStateNodeExist();

    bool isNodeExist(const std::string& node);
    bool isNodeExistForRead(const std::string& node);
    bool isNodeExistForWrite(const std::string& node);

    bool performEffect(Effect effect, EffectStrength strength, int* out_timeout);
    void performEffect_DOUBLE_CLICK();
    bool performHapticEngine(const std::string& packet);
    bool performPrebakedHapticPattern(int index, int timeout, bool repeat);

    bool pollVibeState();
    bool preparePerform();

    std::string readNode(const std::string& node);
    bool writeNode(const std::string& node, int value);
    bool writeNodeByStringOnlyForWriting(const std::string& node, const std::string& value);

    bool setAmplitude(int amplitude);
    bool setPerformAmplitude(int amplitude);

    bool setGpioFallIndex(int index);
    bool setGpioFallScale(int scale);
    bool setGpioRiseIndex(int index);
    bool setGpioRiseScale(int scale);

    bool supportsEnhancedSamsungHapticPattern();

    // Force feedback & CommonInputFF methods
    bool initForceFeedbackEffect();
    bool uploadForceFeedbackeffect();
    bool writeInputEvent(uint16_t type, uint16_t code, int32_t value);
    bool triggerForceFeedbackVibration(int value);
    bool setForceFeedbackIndex(int index);
    bool setForceFeedbackDuration(int duration);
    bool setForceFeedbackAmplitude(int amplitude);
    bool setAndTriggerCommonInputff(const common_inputff_effects* effects);
    std::string get_input_field_value();

    void debug(int fd);

    // Node path accessors
    const std::string& getPrefix() const { return mPrefix; }
    bool isUsingLeds() const { return mUseLeds; }

    const std::string& getIntensityNode() const { return mIntensityNode; }
    const std::string& getForceTouchIntensityNode() const { return mForceTouchIntensityNode; }
    const std::string& getMultiFreqNode() const { return mMultiFreqNode; }
    const std::string& getHapticEngineNode() const { return mHapticEngineNode; }
    const std::string& getCpTriggerIndexNode() const { return mCpTriggerIndexNode; }
    const std::string& getCpTriggerQueueNode() const { return mCpTriggerQueueNode; }
    const std::string& getNumWavesNode() const { return mNumWavesNode; }
    const std::string& getMotorTypeNode() const { return mMotorTypeNode; }
    const std::string& getEventCmdNode() const { return mEventCmdNode; }
    const std::string& getPatternLengthNode() const { return mPatternLengthNode; }
    const std::string& getExtControlNode() const { return mExtControlNode; }
    const std::string& getIntensitiesNode() const { return mIntensitiesNode; }
    const std::string& getHapticIntensitiesNode() const { return mHapticIntensitiesNode; }
    const std::string& getAspEnableNode() const { return mAspEnableNode; }
    const std::string& getFifoNode() const { return mFifoNode; }
    const std::string& getHybridHapticEngineNode() const { return mHybridHapticEngineNode; }
    const std::string& getUseSepIndexNode() const { return mUseSepIndexNode; }
    const std::string& getSupportFunctionsNode() const { return mSupportFunctionsNode; }

  private:
    std::string mPrefix;
    bool mUseLeds;

    std::string mIntensityNode;
    std::string mForceTouchIntensityNode;
    std::string mMultiFreqNode;
    std::string mHapticEngineNode;
    std::string mCpTriggerIndexNode;
    std::string mCpTriggerQueueNode;
    std::string mNumWavesNode;
    std::string mMotorTypeNode;
    std::string mEventCmdNode;
    std::string mPatternLengthNode;
    std::string mExtControlNode;
    std::string mIntensitiesNode;
    std::string mHapticIntensitiesNode;
    std::string mAspEnableNode;
    std::string mGpio1FallIndexNode;
    std::string mGpio1FallDigScaleNode;
    std::string mGpio1RiseIndexNode;
    std::string mGpio1RiseDigScaleNode;
    std::string mVibeStateNode;
    std::string mFifoNode;
    std::string mHybridHapticEngineNode;
    std::string mSupportFunctionsNode;
    std::string mUseSepIndexNode;

    int mMotorIndex;
    bool mCalDynamicIntensities;
    int mAspEnable;
    uint32_t mSupportFeatures;

    int mInputFd;
    int16_t mFfEffectId;
    struct ff_effect mFfEffect;
    std::string mInputDevicePath;

    std::mutex mLock;
};

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
