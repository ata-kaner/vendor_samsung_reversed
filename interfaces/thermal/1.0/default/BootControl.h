/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BOOT_CONTROL_H
#define BOOT_CONTROL_H

#include <string>
#include <vector>

struct HyPerToken {
    uint64_t handle;
};

// Declarations matching libhypervintf.so
extern "C" {
bool hyper_checkResourceExist(int resId);
void hyper_setProcName(std::string procName);
HyPerToken hyper_getNewHyPerToken(void);
std::vector<int> hyper_getSupportedFrequency(int resId, int param);
int hyper_acquire(HyPerToken token, std::vector<int> params);
void hyper_release(HyPerToken token);
}

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

class BootControl {
public:
    BootControl();
    ~BootControl();

    bool check();
    void control(int skinTemp);

private:
    bool checkJig();
    void getFreqMapFromJson(const std::string& path);
    void limit(int freq);
    int approx(int targetFreq);

    bool mEnabled;
    bool mCompleted;
    bool mResourceAvailable;
    int mLastFreq;
    HyPerToken mToken;
    std::vector<int> mSupportedFreqs;
    std::vector<std::pair<int, int>> mFreqMap;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor

#endif  // BOOT_CONTROL_H
