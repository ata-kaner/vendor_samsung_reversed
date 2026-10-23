//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include "BootControl.h"

#include <android/log.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <dlfcn.h>
#include <json/json.h>
#include <fstream>
#include <sstream>

#define LOG_TAG "thermal_core"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

constexpr uint32_t HYPER_RESOURCE_CPU_FREQ = 0x12001002;
constexpr const char* BTCON_PATH = "/vendor/etc/btcon.json";
constexpr const char* JIG_STATE_PATH = "/sys/class/switch/uart3/state";

BootControl::BootControl()
    : mDone(false),
      mInitialized(false),
      mBootCompleted(false),
      mPollCountdown(2),
      mProcName("vendor.samsung.hardware.thermal-service"),
      mToken(0),
      mCurrentStep(0),
      mTargetStep(0),
      mHyperHandle(nullptr),
      mFnSetProcName(nullptr),
      mFnGetNewToken(nullptr),
      mFnCheckResourceExist(nullptr),
      mFnGetSupportedFrequency(nullptr),
      mFnAcquire(nullptr),
      mFnRelease(nullptr) {
    std::string prop = ::android::base::GetProperty("persist.vendor.btcon.disable", "");
    LOGI("btcon disable : %s", prop.c_str());
    if (prop == "1") {
        mDone = true;
    }
}

BootControl::~BootControl() {
    release();
    if (mHyperHandle) {
        dlclose(mHyperHandle);
        mHyperHandle = nullptr;
    }
}

bool BootControl::check_jig() {
    std::ifstream f(JIG_STATE_PATH);
    if (!f.is_open()) {
        return false;
    }
    int c = f.peek();
    LOGI("%s : %d", JIG_STATE_PATH, c);
    return c == '1';
}

bool BootControl::is_ota(const std::string& str) {
    if (str.find("recovery") != std::string::npos ||
        str.find("ota") != std::string::npos ||
        str.find("wssyncmldm") != std::string::npos ||
        str.find("abfotaupdate") != std::string::npos) {
        return true;
    }
    return false;
}

void BootControl::init() {
    if (mInitialized) {
        return;
    }
    mInitialized = true;

    // Load btcon.json
    std::ifstream file(BTCON_PATH);
    if (!file.is_open()) {
        LOGE("Failed to open %s", BTCON_PATH);
        mDone = true;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errs;

    if (!reader->parse(content.data(), content.data() + content.size(), &root, &errs) || !root.isArray()) {
        LOGE("Failed to parse %s: %s", BTCON_PATH, errs.c_str());
        mDone = true;
        return;
    }

    mFreqSteps.clear();
    for (const auto& item : root) {
        if (item.isMember("temp") && item.isMember("freq")) {
            FreqStep step;
            step.temp = item["temp"].asInt();
            step.freq = item["freq"].asInt();
            mFreqSteps.push_back(step);
        }
    }

    if (mFreqSteps.empty()) {
        mDone = true;
        return;
    }

    // Attempt to load libhypervintf.so dynamically
    mHyperHandle = dlopen("libhypervintf.so", RTLD_NOW);
    if (!mHyperHandle) {
        LOGW("libhypervintf.so not found, BootControl disabled");
        mDone = true;
        return;
    }

    mFnSetProcName = reinterpret_cast<void (*)(const char*)>(dlsym(mHyperHandle, "hyper_setProcName"));
    mFnGetNewToken = reinterpret_cast<uint32_t (*)()>(dlsym(mHyperHandle, "hyper_getNewHyPerToken"));
    mFnCheckResourceExist = reinterpret_cast<int (*)(uint32_t)>(dlsym(mHyperHandle, "hyper_checkResourceExist"));
    mFnGetSupportedFrequency = reinterpret_cast<int (*)(uint32_t, int, std::vector<int>*)>(
            dlsym(mHyperHandle, "hyper_getSupportedFrequency"));
    mFnAcquire = reinterpret_cast<int (*)(uint32_t, uint32_t, int)>(dlsym(mHyperHandle, "hyper_acquire"));
    mFnRelease = reinterpret_cast<int (*)(uint32_t)>(dlsym(mHyperHandle, "hyper_release"));

    if (!mFnCheckResourceExist || !mFnCheckResourceExist(HYPER_RESOURCE_CPU_FREQ)) {
        LOGW("Resource 0x%x does not exist in hyper", HYPER_RESOURCE_CPU_FREQ);
        mDone = true;
        return;
    }

    if (mFnSetProcName) {
        mFnSetProcName(mProcName.c_str());
    }
    if (mFnGetNewToken) {
        mToken = mFnGetNewToken();
    }
    if (mFnGetSupportedFrequency) {
        mFnGetSupportedFrequency(HYPER_RESOURCE_CPU_FREQ, 0, &mSupportedFreqs);
    }

    // Map each frequency to closest supported frequency
    for (auto& step : mFreqSteps) {
        if (step.freq > 0) {
            step.freq = approx(step.freq);
        }
    }
}

int BootControl::approx(int targetFreq) {
    if (mSupportedFreqs.empty()) {
        return targetFreq;
    }
    int best = mSupportedFreqs[0];
    int bestDiff = std::abs(targetFreq - best);
    for (int freq : mSupportedFreqs) {
        int diff = std::abs(targetFreq - freq);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = freq;
        }
    }
    LOGI("approx(%d) = %d", targetFreq, best);
    return best;
}

bool BootControl::is_done() {
    if (mDone) {
        return true;
    }

    if (check_jig()) {
        mPollCountdown = 0;
        mBootCompleted = true;
    } else if (!mBootCompleted) {
        std::string prop = ::android::base::GetProperty("vendor.thermal.bt_completed", "");
        LOGI("boot complete : %s", prop.c_str());
        mBootCompleted = (prop == "1");
    }

    if (mBootCompleted) {
        mPollCountdown--;
    }

    if (mPollCountdown <= 0) {
        mDone = true;
    }
    return mDone;
}

void BootControl::control(int skinTemp) {
    if (!mInitialized) {
        init();
    }
    if (mDone || mFreqSteps.empty() || !mFnAcquire) {
        return;
    }

    int stepIdx = 0;
    for (size_t i = 0; i < mFreqSteps.size(); ++i) {
        if (skinTemp >= mFreqSteps[i].temp) {
            stepIdx = static_cast<int>(i);
        }
    }

    mTargetStep = stepIdx;
    if (mTargetStep != mCurrentStep) {
        mCurrentStep = mTargetStep;
        int freq = mFreqSteps[mCurrentStep].freq;
        LOGI("bootcontrol : step %d, freq %d", mCurrentStep, freq);
        if (freq > 0) {
            mFnAcquire(mToken, HYPER_RESOURCE_CPU_FREQ, freq);
        } else {
            release();
        }
    }
}

void BootControl::release() {
    if (mFnRelease && mToken != 0) {
        LOGI("release()");
        mFnRelease(mToken);
        mToken = 0;
    }
}

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
