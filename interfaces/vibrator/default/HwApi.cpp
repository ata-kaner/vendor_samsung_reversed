/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "HwApi.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <thread>
#include <unistd.h>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

static const std::vector<std::pair<std::string, sec_features>> kFeatureMap = {
    {"INTENSITY", INTENSITY},
    {"FORCE_TOUCH_INTENSITY", FORCE_TOUCH_INTENSITY},
    {"MULTI_FREQUENCY", MULTI_FREQUENCY},
    {"HAPTIC_ENGINE", HAPTIC_ENGINE},
    {"INDEX_HAPTIC_PATTERN", INDEX_HAPTIC_PATTERN},
    {"SEPINDEX_HAPTIC_PATTERN", SEPINDEX_HAPTIC_PATTERN},
    {"ENHANCED_HAPTIC_ENGINE", ENHANCED_HAPTIC_ENGINE},
    {"FIFO_HAPTIC_ENGINE", FIFO_HAPTIC_ENGINE},
    {"HYBRID_HAPTIC_ENGINE", HYBRID_HAPTIC_ENGINE},
    {"ENHANCED_HYBRID_HAPTIC_ENGINE", ENHANCED_HYBRID_HAPTIC_ENGINE},
    {"CAPABILITY_EXTERNAL_CONTROL", CAPABILITY_EXTERNAL_CONTROL},
    {"COMMON_INPUTFF_INTERFACE", COMMON_INPUTFF_INTERFACE},
    {"PRIMITIVE_EFFECT_COMPOSE", PRIMITIVE_EFFECT_COMPOSE},
};

static const std::vector<std::pair<std::string, int>> kMotorTypeMap = {
    {"NONE", MOTOR_NONE},
    {"LINEAR_INDEX", MOTOR_LINEAR_INDEX},
    {"COINDC_INDEX", MOTOR_COINDC_INDEX},
    {"COINDC_PMIC", MOTOR_COINDC_PMIC},
    {"LINEAR_0832RC", MOTOR_LINEAR_0832RC},
    {"LINEAR_1040", MOTOR_LINEAR_1040},
    {"LINEAR_RCVMOT", MOTOR_LINEAR_RCVMOT},
    {"LINEAR_1030", MOTOR_LINEAR_1030},
    {"LINEAR_INDEX_RC", MOTOR_LINEAR_INDEX_RC},
    {"LINEAR_WATCH", MOTOR_LINEAR_WATCH},
    {"LINEAR_ACTIVE", MOTOR_LINEAR_ACTIVE},
};

HwApi::HwApi()
    : mUseLeds(false),
      mMotorIndex(0),
      mCalDynamicIntensities(false),
      mAspEnable(0),
      mSupportFeatures(0) {
    if (access("/sys/class/timed_output/vibrator/enable", F_OK) == 0) {
        mUseLeds = false;
        mPrefix = "/sys/class/timed_output/vibrator/";
    } else if (access("/sys/class/leds/vibrator/activate", F_OK) == 0) {
        mUseLeds = true;
        mPrefix = "/sys/class/leds/vibrator/";
        LOG(INFO) << "Vibrator using LED trigger";
    } else {
        mUseLeds = false;
        mPrefix = "/sys/class/timed_output/vibrator/";
    }

    mIntensityNode = mPrefix + "intensity";
    mForceTouchIntensityNode = mPrefix + "force_touch_intensity";
    mMultiFreqNode = mPrefix + "multi_freq";
    mHapticEngineNode = mPrefix + "haptic_engine";
    mCpTriggerIndexNode = mPrefix + "cp_trigger_index";
    mCpTriggerQueueNode = mPrefix + "cp_trigger_queue";
    mNumWavesNode = mPrefix + "num_waves";
    mMotorTypeNode = mPrefix + "motor_type";
    mEventCmdNode = mPrefix + "event_cmd";
    mPatternLengthNode = mPrefix + "pattern_length";
    mExtControlNode = mPrefix + "ext_control";
    mIntensitiesNode = mPrefix + "intensities";
    mHapticIntensitiesNode = mPrefix + "haptic_intensities";
    mAspEnableNode = mPrefix + "asp_enable";
    mGpio1FallIndexNode = mPrefix + "gpio1_fall_index";
    mGpio1FallDigScaleNode = mPrefix + "gpio1_fall_dig_scale";
    mGpio1RiseIndexNode = mPrefix + "gpio1_rise_index";
    mGpio1RiseDigScaleNode = mPrefix + "gpio1_rise_dig_scale";
    mVibeStateNode = mPrefix + "vibe_state";
    mFifoNode = mPrefix + "fifo";
    mHybridHapticEngineNode = mPrefix + "hybrid_haptic_engine";
    mSupportFunctionsNode = "/sys/class/vib_info_class/vib_support_info/functions";

    std::string funcs = readNode(mSupportFunctionsNode);
    for (const auto& [name, feat] : kFeatureMap) {
        if (funcs.find(name) != std::string::npos) {
            mSupportFeatures |= (1u << feat);
        }
    }

    mMotorIndex = getMotorIndex();
    LOG(INFO) << "Vibrator THE_DEVICE_MOTOR_TYPE : " << mMotorIndex;

    mCalDynamicIntensities = isCalDynamicIntensitiesNodeExist();
    LOG(INFO) << "isCalDynamicIntensitiesNodeExist : " << (mCalDynamicIntensities ? "true" : "false");

    if (hasAspEnable()) {
        mAspEnable = getAspEnable();
        LOG(INFO) << "Vibrator THE_DEVICE_ASP_ENABLE : " << mAspEnable;
    }
}

HwApi::~HwApi() {}

bool HwApi::checkClientUID(int uid) {
    if (uid == 1000 || uid == 0) {
        return true;
    }
    LOG(ERROR) << "V_3_0: Pemission denied (" << uid << ")";
    return false;
}

bool HwApi::isNodeExist(const std::string& node) {
    return access(node.c_str(), F_OK) == 0;
}

bool HwApi::isNodeExistForRead(const std::string& node) {
    return access(node.c_str(), R_OK) == 0;
}

bool HwApi::isNodeExistForWrite(const std::string& node) {
    return access(node.c_str(), W_OK) == 0;
}

bool HwApi::writeNode(const std::string& node, int value) {
    std::lock_guard<std::mutex> lock(mLock);
    return ::android::base::WriteStringToFile(std::to_string(value), node);
}

bool HwApi::writeNodeByStringOnlyForWriting(const std::string& node, const std::string& value) {
    std::lock_guard<std::mutex> lock(mLock);
    return ::android::base::WriteStringToFile(value, node);
}

std::string HwApi::readNode(const std::string& node) {
    std::lock_guard<std::mutex> lock(mLock);
    std::string content;
    if (::android::base::ReadFileToString(node, &content)) {
        return ::android::base::Trim(content);
    }
    return "";
}

bool HwApi::do_on(int timeout) {
    if (!mUseLeds) {
        return writeNode(mPrefix + "enable", timeout);
    } else {
        writeNode(mPrefix + "state", 1);
        writeNode(mPrefix + "duration", timeout);
        return writeNode(mPrefix + "activate", 1);
    }
}

bool HwApi::do_off() {
    if (!mUseLeds) {
        return writeNode(mPrefix + "enable", 0);
    } else {
        return writeNode(mPrefix + "activate", 0);
    }
}

int HwApi::getAspEnable() {
    std::string val = readNode(mAspEnableNode);
    if (!val.empty()) {
        return std::stoi(val);
    }
    return 0;
}

bool HwApi::hasAspEnable() {
    return isNodeExist(mAspEnableNode);
}

bool HwApi::setAspEnable(int enable) {
    return writeNode(mAspEnableNode, enable);
}

int HwApi::getMotorIndex() {
    std::string type = readNode(mMotorTypeNode);
    if (!type.empty()) {
        for (const auto& [name, idx] : kMotorTypeMap) {
            if (type == name) {
                return idx;
            }
        }
    }
    return getMotorIndexbyMethod();
}

int HwApi::getMotorIndexbyMethod() {
    if (isNumWavesNodeExist()) {
        if (isHapticEngineNodeExist()) {
            return MOTOR_LINEAR_1040;
        } else {
            return MOTOR_LINEAR_WATCH;
        }
    } else {
        if (isHapticEngineNodeExist()) {
            if (isCalDynamicIntensitiesNodeExist()) {
                return MOTOR_LINEAR_RCVMOT;
            } else {
                return MOTOR_LINEAR_1030;
            }
        } else {
            if (isAmplitudeControlExist()) {
                return MOTOR_LINEAR_INDEX;
            } else {
                return MOTOR_COINDC_INDEX;
            }
        }
    }
}

int HwApi::getMotorPerformMethod() {
    if (isNodeExist(mCpTriggerIndexNode)) {
        return 1;
    } else if (isNodeExist(mHapticEngineNode)) {
        return 2;
    } else if (isNodeExist(mIntensityNode)) {
        return 3;
    }
    return 0;
}

bool HwApi::hasFeature(sec_features feature) {
    return (mSupportFeatures & (1u << feature)) != 0;
}

bool HwApi::isAmplitudeControlExist() {
    return isNodeExist(mIntensityNode);
}

bool HwApi::isCalDynamicIntensitiesNodeExist() {
    return isNodeExist(mHapticIntensitiesNode) || isNodeExist(mIntensitiesNode);
}

bool HwApi::isHapticEngineNodeExist() {
    return isNodeExist(mHapticEngineNode);
}

bool HwApi::isMotorTypeNodeExist() {
    return isNodeExist(mMotorTypeNode);
}

bool HwApi::isNumWavesNodeExist() {
    return isNodeExist(mNumWavesNode);
}

bool HwApi::isPrebakedHapticPatternExist() {
    return false;
}

bool HwApi::isVibrating() {
    if (isNodeExist(mVibeStateNode)) {
        std::string state = readNode(mVibeStateNode);
        return state == "1";
    }
    return false;
}

bool HwApi::isVibratorActiveNodeExist() {
    return isNodeExist(mPrefix + "activate");
}

bool HwApi::isVibratorDurationNodeExist() {
    return isNodeExist(mPrefix + "duration");
}

bool HwApi::isVibratorNodeExist() {
    return isNodeExist(mPrefix + "enable");
}

bool HwApi::isVibratorStateNodeExist() {
    return isNodeExist(mPrefix + "state");
}

bool HwApi::performEffect(Effect effect, EffectStrength strength, int* out_timeout) {
    int method = getMotorPerformMethod();

    if (method == 1) {
        // Trigger Index Mode
        int index = 0;
        switch (effect) {
            case Effect::CLICK:
                index = 10;
                break;
            case Effect::DOUBLE_CLICK:
                index = 14;
                break;
            case Effect::TICK:
                index = 11;
                break;
            case Effect::HEAVY_CLICK:
                index = 23;
                break;
            default:
                return false;
        }
        writeNode(mCpTriggerIndexNode, index);
        do_on(110);
        if (out_timeout) *out_timeout = 110;
        return true;
    } else if (method == 2) {
        // Haptic Engine Packet Mode
        int timeout = 17;
        int intensity = 10000;
        switch (strength) {
            case EffectStrength::LIGHT:
                intensity = 3000;
                break;
            case EffectStrength::MEDIUM:
                intensity = 6000;
                break;
            case EffectStrength::STRONG:
            default:
                intensity = 10000;
                break;
        }
        writeNode(mIntensityNode, intensity);
        do_on(timeout);
        if (out_timeout) *out_timeout = timeout;
        return true;
    } else if (method == 3) {
        // Direct Intensity Mode
        int intensity = 10000;
        switch (strength) {
            case EffectStrength::LIGHT:
                intensity = 2500;
                break;
            case EffectStrength::MEDIUM:
                intensity = 5000;
                break;
            case EffectStrength::STRONG:
            default:
                intensity = 10000;
                break;
        }
        writeNode(mIntensityNode, intensity);

        if (effect == Effect::DOUBLE_CLICK) {
            performEffect_DOUBLE_CLICK();
            if (out_timeout) *out_timeout = 134;
            return true;
        }

        do_on(17);
        if (out_timeout) *out_timeout = 17;
        return true;
    }

    return false;
}

void HwApi::performEffect_DOUBLE_CLICK() {
    std::thread([this]() {
        do_on(17);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        do_on(17);
    }).detach();
}

bool HwApi::performHapticEngine(const std::string& packet) {
    return writeNodeByStringOnlyForWriting(mHapticEngineNode, packet);
}

bool HwApi::performPrebakedHapticPattern(int index, int timeout, bool repeat) {
    if (repeat) {
        writeNode(mNumWavesNode, 0xFFFE);
        std::string cmd = std::to_string(index) + "." + std::to_string(timeout / 100) + ", 10, 0~";
        return writeNodeByStringOnlyForWriting(mHapticEngineNode, cmd);
    } else {
        writeNode(mIntensityNode, timeout);
        writeNode(mNumWavesNode, index);
        if (index != 0) {
            return do_on(1000);
        }
        return true;
    }
}

bool HwApi::pollVibeState() {
    int count = 0;
    while (isVibrating() && count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        count++;
    }
    return !isVibrating();
}

bool HwApi::preparePerform() {
    return true;
}

bool HwApi::setAmplitude(int amplitude) {
    return writeNode(mIntensityNode, amplitude);
}

bool HwApi::setPerformAmplitude(int amplitude) {
    return writeNode(mIntensityNode, amplitude);
}

bool HwApi::setGpioFallIndex(int index) {
    return writeNode(mGpio1FallIndexNode, index);
}

bool HwApi::setGpioFallScale(int scale) {
    return writeNode(mGpio1FallDigScaleNode, scale);
}

bool HwApi::setGpioRiseIndex(int index) {
    return writeNode(mGpio1RiseIndexNode, index);
}

bool HwApi::setGpioRiseScale(int scale) {
    return writeNode(mGpio1RiseDigScaleNode, scale);
}

bool HwApi::supportsEnhancedSamsungHapticPattern() {
    return mMotorIndex == MOTOR_LINEAR_1040 || isHapticEngineNodeExist();
}

void HwApi::debug(int fd) {
    dprintf(fd, "HwApi Debug Info:\n");
    dprintf(fd, "  Prefix: %s\n", mPrefix.c_str());
    dprintf(fd, "  Use LEDs: %s\n", mUseLeds ? "true" : "false");
    dprintf(fd, "  Motor Index: %d\n", mMotorIndex);
    dprintf(fd, "  Dynamic Intensities: %s\n", mCalDynamicIntensities ? "true" : "false");
    dprintf(fd, "  ASP Enable: %d\n", mAspEnable);
    dprintf(fd, "  Features bitmask: 0x%08x\n", mSupportFeatures);
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
