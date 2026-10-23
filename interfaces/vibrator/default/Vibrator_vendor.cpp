/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Vibrator_vendor.h"

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android/binder_ibinder.h>

#include <chrono>
#include <sstream>
#include <thread>

using aidl::android::hardware::vibrator::common_inputff_effects;
using aidl::android::hardware::vibrator::HYBRID_PATTERN_COMMON_INPUTFF;

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace vibrator {

Vibrator::Vibrator(std::shared_ptr<aidl::android::hardware::vibrator::HwApi> hwApi)
    : mHwApi(std::move(hwApi)) {
    LOG(INFO) << "Vibrator::Vendor Vibrator()";
}

ndk::ScopedAStatus Vibrator::supportsAmplitudeControl(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = mHwApi->isAmplitudeControlExist();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsForceTouchAmplitudeControl(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = mHwApi->isNodeExist(mHwApi->getForceTouchIntensityNode());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsFrequencyControl(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = mHwApi->isNodeExist(mHwApi->getMultiFreqNode());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsHapticEngine(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = mHwApi->isHapticEngineNodeExist();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsPrebakedHapticPattern(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = false;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsFolderStatusSetting(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }

    std::string content = mHwApi->readNode(mHwApi->getEventCmdNode());
    if (content.find("FOLDER") != std::string::npos) {
        LOG(INFO) << "supportsFolderStatusSetting TRUE";
        *_aidl_return = true;
    } else {
        LOG(INFO) << "supportsFolderStatusSetting FALSE";
        *_aidl_return = false;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsEnhancedSamsungHapticPattern(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = mHwApi->supportsEnhancedSamsungHapticPattern();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(int32_t amplitude) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    if (amplitude < 1 || amplitude > 10000) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    mHwApi->setAmplitude(amplitude);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setForceTouchAmplitude(int32_t amplitude) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    if (amplitude < 1 || amplitude > 10000) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    LOG(INFO) << "setForceTouchAmplitude " << amplitude;
    mHwApi->writeNode(mHwApi->getForceTouchIntensityNode(), amplitude);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setFrequencyType(SehFrequencyType type) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    int val = static_cast<int>(type);
    if (val < 0 || val >= 5) {
        LOG(ERROR) << "V_5_0: setFrequencyType is abnormal " << val;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    LOG(INFO) << "V_5_0: setFrequencyType " << val;
    mHwApi->writeNode(mHwApi->getMultiFreqNode(), val);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setFolderStatus(SehFolderStatus status) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    std::string cmd;
    if (status == SehFolderStatus::CLOSE) {
        cmd = "FOLDER_CLOSE\n";
    } else if (status == SehFolderStatus::OPEN) {
        cmd = "FOLDER_OPEN\n";
    } else {
        LOG(ERROR) << "V_5_0: setFolderStatus enum error";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!mHwApi->writeNodeByStringOnlyForWriting(mHwApi->getEventCmdNode(), cmd)) {
        LOG(ERROR) << "V_5_0: setFolderStatus is fail: " << static_cast<int>(status);
    }
    LOG(INFO) << "setFolderStatus end";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getNumberOfPrebakedHapticPattern(int32_t* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = 0;
        return ndk::ScopedAStatus::ok();
    }

    std::string content = mHwApi->readNode(mHwApi->getNumWavesNode());
    if (!content.empty()) {
        try {
            *_aidl_return = std::stoi(content);
        } catch (...) {
            *_aidl_return = 0;
        }
    } else {
        *_aidl_return = 0;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performHapticEngine(
        const std::vector<SehHapticEnginePacket>& packets, int32_t repeat) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    if (packets.empty()) {
        return ndk::ScopedAStatus::ok();
    }

    if (packets.size() * 4 > 3400) {
        LOG(ERROR) << "V_5_0: performHapticEngine packet size is exceed";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::string buffer = std::to_string(packets.size() * 4) + " ";
    for (const auto& packet : packets) {
        int scaledIntensity = (packet.data2 * repeat) / 10000;
        buffer += std::to_string(packet.data1) + " ";
        buffer += std::to_string(scaledIntensity) + " ";
        buffer += std::to_string(packet.data3) + " ";
        buffer += std::to_string(packet.data4) + " ";
    }

    if (mHwApi->writeNodeByStringOnlyForWriting(mHwApi->getHapticEngineNode(), buffer)) {
        LOG(INFO) << "performHapticEngine OK";
    } else {
        LOG(ERROR) << "performHapticEngine UNKNOWN_ERROR";
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performPrebakedHapticPattern(int32_t index, int32_t timeout,
                                                         bool repeat) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    if (!mHwApi->performPrebakedHapticPattern(index, timeout, repeat)) {
        LOG(ERROR) << "sehPerformPrebakedHapticPattern repeat error";
    }
    LOG(INFO) << "performPrebakedHapticPattern one shoot end";
    return ndk::ScopedAStatus::ok();
}

bool Vibrator::__getDynamicAmplitudeList(SehAmplitudeType type,
                                         std::vector<int32_t>* _aidl_return) {
    std::string node;
    if (type == SehAmplitudeType::BIASED) {
        node = mHwApi->getHapticIntensitiesNode();
    } else if (type == SehAmplitudeType::DYNAMIC) {
        node = mHwApi->getIntensitiesNode();
    } else {
        LOG(ERROR) << "V_5_0: getDynamicAmplitudeList amplitude type is worng";
        return false;
    }

    LOG(INFO) << "V_5_0: getDynamicAmplitudeList node: " << node;
    std::string content = mHwApi->readNode(node);
    if (content.empty()) {
        LOG(WARNING) << "getDynamicAmplitudeList is empty";
        return false;
    }

    LOG(INFO) << "getDynamicAmplitudeList intensities : " << content;
    std::stringstream ss(content);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = ::android::base::Trim(item);
        if (!item.empty()) {
            try {
                _aidl_return->push_back(std::stoi(item));
            } catch (...) {}
        }
    }

    return !_aidl_return->empty();
}

ndk::ScopedAStatus Vibrator::getAmplitudeList(SehAmplitudeType type,
                                            std::vector<int32_t>* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    LOG(INFO) << "V_5_0: getAmplitudeList amplitude type: " << static_cast<int>(type);

    if (__getDynamicAmplitudeList(type, _aidl_return)) {
        LOG(INFO) << "getAmplitudeList size: " << _aidl_return->size();
        return ndk::ScopedAStatus::ok();
    }

    LOG(INFO) << "DynamicAmplitude is support but try to fallback routine.";
    int motorIdx = mHwApi->getMotorIndex();
    LOG(INFO) << "sehGetAmplitudeList existing_motorType : " << motorIdx;

    switch (motorIdx) {
        case 1:
        case 4:
            *_aidl_return = {0, 3000, 4000, 6000, 8000, 10000};
            break;
        case 2:
        case 8:
        case 9:
            *_aidl_return = {0, 2000, 4000, 6000, 8000, 10000};
            break;
        case 3:
            *_aidl_return = {0, 1000, 5000, 9000};
            break;
        case 5:
        case 6:
        case 7:
            *_aidl_return = {0, 3000, 4000, 6000, 8000, 10000};
            break;
        case 10:
            *_aidl_return = {0, 1500, 4000, 6000, 8000, 10000};
            break;
        case 11:
        case 12:
            *_aidl_return = {0, 2000, 6000, 10000};
            break;
        default:
            *_aidl_return = {0, 2000, 4000, 6000, 8000, 10000};
            break;
    }

    LOG(INFO) << "getAmplitudeList size: " << _aidl_return->size();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::perform(
        aidl::android::hardware::vibrator::Effect effect, int32_t strength,
        const std::shared_ptr<aidl::android::hardware::vibrator::IVibratorCallback>& callback,
        int32_t* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = 0;
        return ndk::ScopedAStatus::ok();
    }

    auto effectStrength = static_cast<aidl::android::hardware::vibrator::EffectStrength>(strength);
    if (!mHwApi->performEffect(effect, effectStrength, _aidl_return)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    if (callback != nullptr) {
        int timeout = *_aidl_return;
        std::thread([timeout, callback]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            callback->onComplete();
        }).detach();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performPlaybackEngine(
        const std::vector<SehPlaybackPacket>& packets, bool repeat, int32_t timeout) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    if (packets.empty()) {
        return ndk::ScopedAStatus::ok();
    }

    std::string cmd;
    for (size_t i = 0; i < packets.size(); ++i) {
        const auto& packet = packets[i];
        cmd += std::to_string(packet.data1) + "." + std::to_string(packet.data2);
        if (packet.data3 != 0) {
            cmd += "," + std::to_string(packet.data3) + ",";
        } else if (i + 1 < packets.size()) {
            cmd += ",";
        }
    }

    if (repeat) {
        if (timeout > 0) {
            cmd += std::to_string(timeout) + "!";
        } else {
            cmd += "~";
        }
    } else {
        if (!cmd.empty() && cmd.back() == ',') {
            cmd.pop_back();
        }
    }

    LOG(INFO) << "performPlaybackEngine repeat : " << (repeat ? "true" : "false");
    if (!mHwApi->writeNodeByStringOnlyForWriting(mHwApi->getHapticEngineNode(), cmd)) {
        LOG(ERROR) << "performPlaybackEngine on Trigger fail";
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsFifoHapticEngine(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = mHwApi->isNodeExist(mHwApi->getFifoNode());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsHybridHapticEngine(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = mHwApi->isNodeExist(mHwApi->getHybridHapticEngineNode());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsEnhancedHybridHapticPattern(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = (mHwApi->getMotorIndex() == 11);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performFifoHapticEngine(SehFifoType type) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    int val = static_cast<int>(type);
    if (val < 0 || val >= 26) {
        LOG(ERROR) << "V_5_0: performFifoHapticEngine is abnormal " << val;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    mHwApi->writeNode(mHwApi->getFifoNode(), val);
    LOG(INFO) << "V_5_0: performFIFOHapticEngine OK";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performHybridHapticEngine(
        const std::vector<SehHybridHapticEnginePacket>& packets, int32_t repeat) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    if (packets.empty()) {
        return ndk::ScopedAStatus::ok();
    }

    LOG(INFO) << "performHybridHapticEngine packet_cnt : " << packets.size();

    std::string buffer = std::to_string(packets.size() * 5) + " ";
    for (const auto& packet : packets) {
        int scaledIntensity = (packet.data2 * repeat) / 10000;
        buffer += std::to_string(packet.data1) + " ";
        buffer += std::to_string(scaledIntensity) + " ";
        buffer += std::to_string(packet.data3) + " ";
        buffer += std::to_string(packet.data4) + " ";
        buffer += std::to_string(packet.data5) + " ";
    }

    if (!mHwApi->writeNodeByStringOnlyForWriting(mHwApi->getHybridHapticEngineNode(), buffer)) {
        LOG(ERROR) << "performHybridHapticEngine is fail: " << buffer;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsFolderTentModeStatusSetting(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }

    std::string content = mHwApi->readNode(mHwApi->getEventCmdNode());
    if (content.find("FOLDER_TENT") != std::string::npos) {
        LOG(INFO) << "supportsFolderTentModeStatusSetting TRUE";
        *_aidl_return = true;
    } else {
        LOG(INFO) << "supportsFolderTentModeStatusSetting FALSE";
        *_aidl_return = false;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setFolderTentModeStatus(SehFolderTentModeStatus status) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::ok();
    }

    std::string cmd;
    if (status == SehFolderTentModeStatus::CLOSE) {
        cmd = "FOLDER_TENT_CLOSE\n";
    } else if (status == SehFolderTentModeStatus::OPEN) {
        cmd = "FOLDER_TENT_OPEN\n";
    } else {
        LOG(ERROR) << "V_5_0: setFolderTentModeStatus enum error";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!mHwApi->writeNodeByStringOnlyForWriting(mHwApi->getEventCmdNode(), cmd)) {
        LOG(ERROR) << "setFolderTentModeStatus is fail: " << static_cast<int>(status);
    }
    LOG(INFO) << "setFolderTentModeStatus end";
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::supportsHasFeature(bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }
    *_aidl_return = mHwApi->supportsHasFeature();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::hasFeature(const std::string& feature, bool* _aidl_return) {
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }
    if (feature.empty()) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (!mHwApi->supportsHasFeature()) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    *_aidl_return = mHwApi->hasFeature(feature);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performCommonInputff(
        const std::vector<SehCommonInputffPacket>& packets, bool repeat, int32_t timeout) {
    LOG(DEBUG) << "[performCommonInputff]";
    if (!mHwApi->checkClientUID(AIBinder_getCallingUid())) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
    }
    if (!mHwApi->hasFeature(HYBRID_PATTERN_COMMON_INPUTFF)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    if (packets.empty() || packets.size() > 32) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (timeout < 1 || timeout > 10000) {
        LOG(ERROR) << "Duration too long: " << timeout << " ms";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    common_inputff_effects effects;
    memset(&effects, 0, sizeof(effects));
    effects.count = packets.size();
    effects.repeat = repeat ? 1 : 0;

    for (size_t i = 0; i < packets.size(); ++i) {
        effects.packets[i].data1 = packets[i].data1;
        effects.packets[i].data2 = packets[i].data2;
        effects.packets[i].data3 = packets[i].data3;
        effects.packets[i].data4 = packets[i].data4;
        effects.packets[i].data5 = packets[i].data5;
    }

    if (!mHwApi->setAndTriggerCommonInputff(&effects)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
