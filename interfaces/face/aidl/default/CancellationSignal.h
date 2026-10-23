/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/common/BnCancellationSignal.h>
#include <aidl/android/hardware/biometrics/face/ISessionCallback.h>
#include <future>
#include <memory>

namespace android {
class FaceAIDLService;
}

namespace aidl::android::hardware::biometrics::face {

class CancellationSignal : public common::BnCancellationSignal {
public:
    CancellationSignal(std::shared_future<void> future,
                       uint32_t cmdId,
                       std::shared_ptr<::android::FaceAIDLService> faceAIDLService,
                       std::shared_ptr<ISessionCallback> cb);
    virtual ~CancellationSignal() = default;

    ndk::ScopedAStatus cancel() override;

private:
    std::shared_future<void> mFuture;
    std::weak_ptr<::android::FaceAIDLService> mFaceAIDLService;
    std::weak_ptr<ISessionCallback> mCallback;
    uint32_t mCmdId;
};

} // namespace aidl::android::hardware::biometrics::face
