/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "BootControl.h"

#include <android/log.h>
#include <android-base/properties.h>
#include <json/json.h>
#include <sys/system_properties.h>

#include <algorithm>
#include <cmath>
#include <fstream>

#define LOG_TAG "SehThermal-HAL"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

constexpr const char* kBtconJsonPath = "/vendor/etc/btcon.json";
constexpr const char* kJigStatePath = "/sys/class/switch/uart3/state";
constexpr const char* kPropBtconDisable = "persist.vendor.btcon.disable";
constexpr const char* kPropBtCompleted = "vendor.thermal.bt_completed";
constexpr int kCpuClusterResId = 0x12001002;

BootControl::BootControl()
    : mEnabled(false),
      mCompleted(false),
      mResourceAvailable(false),
      mLastFreq(-1),
      mToken{} {
    char propVal[PROP_VALUE_MAX] = {0};
    __system_property_get(kPropBtconDisable, propVal);
    LOGW("btcon disable : %s", propVal);

    if (propVal[0] == '1') {
        LOGI("BootControl disabled via property");
        return;
    }

    if (checkJig()) {
        LOGI("BootControl disabled: factory JIG detected");
        return;
    }

    getFreqMapFromJson(kBtconJsonPath);
    if (mFreqMap.empty()) {
        LOGW("BootControl: no frequency mapping loaded");
        return;
    }

    mResourceAvailable = hyper_checkResourceExist(kCpuClusterResId);
    if (!mResourceAvailable) {
        LOGW("BootControl: HyPer resource 0x%x not available", kCpuClusterResId);
        return;
    }

    hyper_setProcName("vendor.samsung.hardware.thermal");
    mToken = hyper_getNewHyPerToken();
    mSupportedFreqs = hyper_getSupportedFrequency(kCpuClusterResId, 0);

    for (auto& entry : mFreqMap) {
        int target = entry.second;
        if (target > 0) {
            entry.second = approx(target);
            LOGD("approx(%d) = %d", target, entry.second);
        }
    }

    mEnabled = true;
    LOGI("BootControl initialized successfully");
}

BootControl::~BootControl() {
    if (mEnabled) {
        limit(-1);
    }
}

bool BootControl::checkJig() {
    std::ifstream jigFile(kJigStatePath);
    if (!jigFile.is_open()) {
        return false;
    }

    char state = '0';
    jigFile >> state;
    return state == '1';
}

void BootControl::getFreqMapFromJson(const std::string& path) {
    std::ifstream jsonFile(path);
    if (!jsonFile.is_open()) {
        LOGE("Failed to open %s", path.c_str());
        return;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;

    if (!Json::parseFromStream(builder, jsonFile, &root, &errs) || !root.isArray()) {
        LOGE("Failed to parse %s: %s", path.c_str(), errs.c_str());
        return;
    }

    for (const auto& item : root) {
        if (item.isMember("temp") && item.isMember("freq")) {
            int temp = item["temp"].asInt();
            int freq = item["freq"].asInt();
            mFreqMap.emplace_back(temp, freq);
        }
    }

    std::sort(mFreqMap.begin(), mFreqMap.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
}

int BootControl::approx(int targetFreq) {
    if (mSupportedFreqs.empty()) {
        return targetFreq;
    }

    int bestFreq = mSupportedFreqs[0];
    int minDiff = std::abs(targetFreq - bestFreq);

    for (int freq : mSupportedFreqs) {
        int diff = std::abs(targetFreq - freq);
        if (diff < minDiff) {
            minDiff = diff;
            bestFreq = freq;
        }
    }
    return bestFreq;
}

bool BootControl::check() {
    if (mCompleted || !mEnabled) {
        return true;
    }

    if (android::base::GetProperty(kPropBtCompleted, "") == "1") {
        LOGI("Boot completed signal received, disabling BootControl");
        limit(-1);
        hyper_release(mToken);
        mCompleted = true;
        mEnabled = false;
        return true;
    }

    return false;
}

void BootControl::control(int skinTemp) {
    if (!mEnabled || mCompleted) {
        return;
    }

    if (check()) {
        return;
    }

    int targetFreq = -1;
    for (const auto& entry : mFreqMap) {
        if (skinTemp >= entry.first) {
            targetFreq = entry.second;
        }
    }

    if (targetFreq != mLastFreq) {
        limit(targetFreq);
        mLastFreq = targetFreq;
    }
}

void BootControl::limit(int freq) {
    if (!mResourceAvailable) {
        return;
    }

    if (freq <= 0) {
        hyper_release(mToken);
    } else {
        std::vector<int> params = {kCpuClusterResId, freq};
        hyper_acquire(mToken, params);
    }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
