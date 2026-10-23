//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

struct FreqStep {
    int temp;
    int freq;
};

class BootControl {
  public:
    BootControl();
    ~BootControl();

    void init();
    bool is_done();
    void control(int skinTemp);
    void release();

    static bool check_jig();
    static bool is_ota(const std::string& str);

  private:
    bool mDone;
    bool mInitialized;
    bool mBootCompleted;
    int mPollCountdown;
    std::string mProcName;
    uint32_t mToken;
    std::vector<int> mSupportedFreqs;
    std::vector<FreqStep> mFreqSteps;
    int mCurrentStep;
    int mTargetStep;

    void* mHyperHandle;
    void (*mFnSetProcName)(const char*);
    uint32_t (*mFnGetNewToken)();
    int (*mFnCheckResourceExist)(uint32_t);
    int (*mFnGetSupportedFrequency)(uint32_t, int, std::vector<int>*);
    int (*mFnAcquire)(uint32_t, uint32_t, int);
    int (*mFnRelease)(uint32_t);

    int approx(int targetFreq);
};

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
