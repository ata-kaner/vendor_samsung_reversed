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
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/time.h>
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
    {"HYBRID_PATTERN_COMMON_INPUTFF", HYBRID_PATTERN_COMMON_INPUTFF},
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
      mSupportFeatures(0),
      mInputFd(-1),
      mFfEffectId(-1) {
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
    mUseSepIndexNode = "/sys/class/sec_vib_inputff/control/use_sep_index";

    memset(&mFfEffect, 0, sizeof(mFfEffect));

    std::string funcs = readNode(mSupportFunctionsNode);
    for (const auto& [name, feat] : kFeatureMap) {
        if (funcs.find(name) != std::string::npos) {
            mSupportFeatures |= (1u << feat);
        }
    }

    mMotorIndex = getMotorIndex();
    LOG(INFO) << "Vibrator THE_DEVICE_MOTOR_TYPE : " << mMotorIndex;

    mCalDynamicIntensities = isCalDynamicIntensitiesNodeExist();
    LOG(INFO) << "isCalDynamicIntensitiesNodeExist : "
              << (mCalDynamicIntensities ? "true" : "false");

    if (hasAspEnable()) {
        mAspEnable = getAspEnable();
        LOG(INFO) << "Vibrator THE_DEVICE_ASP_ENABLE : " << mAspEnable;
    }

    if (hasFeature(COMMON_INPUTFF_INTERFACE) ||
        hasFeature(HYBRID_PATTERN_COMMON_INPUTFF) ||
        access(mUseSepIndexNode.c_str(), F_OK) == 0) {
        initForceFeedbackEffect();
    }
}

HwApi::~HwApi() {
    if (mInputFd >= 0) {
        if (mFfEffectId != -1) {
            ioctl(mInputFd, EVIOCRMFF, mFfEffectId);
            mFfEffectId = -1;
        }
        close(mInputFd);
        mInputFd = -1;
        LOG(INFO) << "inputFD close";
    }
}

bool HwApi::checkClientUID(int uid) {
    if (uid == 1000 || uid == 0) {
        return true;
    }
    LOG(ERROR) << "V_5_0: Pemission denied (" << uid << ")";
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

bool HwApi::hasFeature(const std::string& feature) {
    LOG(DEBUG) << "[hasFeature] feature: " << feature;
    if (feature.empty() || !isNodeExist(mSupportFunctionsNode)) {
        return false;
    }
    std::string allFeatures = readNode(mSupportFunctionsNode);
    if (allFeatures.empty()) {
        return false;
    }
    std::istringstream stream(allFeatures);
    std::string token;
    while (std::getline(stream, token, ' ')) {
        if (token == feature) {
            return true;
        }
    }
    return false;
}

bool HwApi::supportsHasFeature() {
    return isNodeExist(mSupportFunctionsNode);
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

std::string HwApi::get_input_field_value() {
    FILE* fp = fopen("/proc/bus/input/devices", "r");
    if (!fp) {
        LOG(ERROR) << "Failed to open /proc/bus/input/devices: " << strerror(errno);
        return "";
    }

    char line[1024];
    bool foundDevice = false;
    std::string eventNode;

    while (fgets(line, sizeof(line), fp)) {
        if (!foundDevice) {
            if (strstr(line, "N: Name=\"sec_vibrator_inputff\"")) {
                foundDevice = true;
            }
        } else {
            if (line[0] == '\n') {
                foundDevice = false;
                continue;
            }
            if (line[0] == 'H') {
                char* p = strstr(line, "event");
                if (p) {
                    char eventName[64] = {0};
                    if (sscanf(p, "%63s", eventName) == 1) {
                        eventNode = std::string("/dev/input/") + eventName;
                        break;
                    }
                }
            }
        }
    }
    fclose(fp);
    return eventNode;
}

bool HwApi::initForceFeedbackEffect() {
    LOG(DEBUG) << "[initForceFeedbackEffect]";
    if (mInputFd >= 0) {
        return true;
    }

    mInputDevicePath = get_input_field_value();
    if (mInputDevicePath.empty()) {
        LOG(WARNING) << "[initForceFeedbackEffect] sec_vibrator_inputff not found in /proc/bus/input/devices";
        return false;
    }

    mInputFd = open(mInputDevicePath.c_str(), O_RDWR | O_CLOEXEC);
    if (mInputFd < 0) {
        LOG(ERROR) << "[initForceFeedbackEffect] Failed to open " << mInputDevicePath
                   << ": " << strerror(errno);
        return false;
    }

    LOG(INFO) << "Input Node File Open: " << mInputDevicePath;
    writeNodeByStringOnlyForWriting(mUseSepIndexNode, "1");

    memset(&mFfEffect, 0, sizeof(mFfEffect));
    mFfEffect.type = FF_PERIODIC;
    mFfEffect.id = -1;
    mFfEffect.replay.length = 0x5d;
    mFfEffect.u.periodic.waveform = FF_CUSTOM;

    if (!uploadForceFeedbackeffect()) {
        LOG(ERROR) << "[initForceFeedbackEffect] initial upload failed";
        return false;
    }

    LOG(INFO) << "Vibrator using ForceFeedback";
    return true;
}

bool HwApi::uploadForceFeedbackeffect() {
    LOG(DEBUG) << "[uploadForceFeedbackeffect]";
    if (mInputFd < 0) {
        return false;
    }
    if (mFfEffectId != -1) {
        ioctl(mInputFd, EVIOCRMFF, mFfEffectId);
        mFfEffectId = -1;
    }
    int ret = ioctl(mInputFd, EVIOCSFF, &mFfEffect);
    if (ret < 0) {
        LOG(ERROR) << "[uploadForceFeedbackeffect] ioctl failed: " << strerror(errno);
        return false;
    }
    mFfEffectId = mFfEffect.id;
    LOG(DEBUG) << "ioctl success";
    LOG(DEBUG) << "THE_DEVICE_FF: " << mFfEffectId;
    return true;
}

bool HwApi::writeInputEvent(uint16_t type, uint16_t code, int32_t value) {
    LOG(DEBUG) << "[writeInputEvent]";
    if (mInputFd < 0) {
        return false;
    }
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    gettimeofday(&ev.time, nullptr);
    ev.type = type;
    ev.code = code;
    ev.value = value;
    ssize_t ret = TEMP_FAILURE_RETRY(write(mInputFd, &ev, sizeof(ev)));
    return ret == sizeof(ev);
}

bool HwApi::triggerForceFeedbackVibration(int value) {
    LOG(DEBUG) << "[triggerForceFeedbackVibration]";
    if (mFfEffectId < 0) {
        return false;
    }
    return writeInputEvent(EV_FF, mFfEffectId, value);
}

bool HwApi::setForceFeedbackIndex(int index) {
    mFfEffect.u.periodic.waveform = FF_CUSTOM;
    mFfEffect.u.periodic.period = index;
    return uploadForceFeedbackeffect();
}

bool HwApi::setForceFeedbackDuration(int duration) {
    mFfEffect.replay.length = duration;
    return uploadForceFeedbackeffect();
}

bool HwApi::setForceFeedbackAmplitude(int amplitude) {
    LOG(DEBUG) << "[setForceFeedbackAmplitude]";
    mFfEffect.u.periodic.magnitude = amplitude;
    return uploadForceFeedbackeffect();
}

bool HwApi::setAndTriggerCommonInputff(const common_inputff_effects* effects) {
    LOG(DEBUG) << "[setAndTriggerCommonInputff]";
    if (!effects) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mLock);
    if (mInputFd < 0) {
        if (!initForceFeedbackEffect()) {
            return false;
        }
    }
    mFfEffect.type = FF_PERIODIC;
    mFfEffect.id = mFfEffectId;
    mFfEffect.u.periodic.custom_len = sizeof(common_inputff_effects);
    mFfEffect.u.periodic.custom_data = reinterpret_cast<__s16*>(const_cast<common_inputff_effects*>(effects));

    if (!uploadForceFeedbackeffect()) {
        return false;
    }
    bool ok = triggerForceFeedbackVibration(1);
    mFfEffect.u.periodic.custom_len = 2;
    mFfEffect.u.periodic.custom_data = nullptr;
    return ok;
}

void HwApi::debug(int fd) {
    dprintf(fd, "HwApi Debug Info:\n");
    dprintf(fd, "  Prefix: %s\n", mPrefix.c_str());
    dprintf(fd, "  Use LEDs: %s\n", mUseLeds ? "true" : "false");
    dprintf(fd, "  Motor Index: %d\n", mMotorIndex);
    dprintf(fd, "  Dynamic Intensities: %s\n", mCalDynamicIntensities ? "true" : "false");
    dprintf(fd, "  ASP Enable: %d\n", mAspEnable);
    dprintf(fd, "  Features bitmask: 0x%08x\n", mSupportFeatures);
    dprintf(fd, "  Input FD: %d\n", mInputFd);
    dprintf(fd, "  Input Device Path: %s\n", mInputDevicePath.c_str());
    dprintf(fd, "  FF Effect ID: %d\n", mFfEffectId);
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
