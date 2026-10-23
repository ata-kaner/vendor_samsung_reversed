/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CancellationSignal.h"
#include "FaceAIDLService.h"

#include <aidl/android/hardware/biometrics/face/Error.h>
#include <android-base/logging.h>

#undef LOG_TAG
#define LOG_TAG "sec_fr_CancellationSignal"

namespace aidl::android::hardware::biometrics::face {

CancellationSignal::CancellationSignal(std::shared_future<void> future,
                                       uint32_t cmdId,
                                       std::shared_ptr<::android::FaceAIDLService> faceAIDLService,
                                       std::shared_ptr<ISessionCallback> cb)
    : mFuture(std::move(future)),
      mFaceAIDLService(faceAIDLService),
      mCallback(cb),
      mCmdId(cmdId) {}

ndk::ScopedAStatus CancellationSignal::cancel() {
    LOG(DEBUG) << "CancellationSignal::cancel(mCmdID=" << mCmdId << ")";

    if (mFuture.valid() && mFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        LOG(DEBUG) << "libFaceService is already cancelled";
        auto cb = mCallback.lock();
        if (cb != nullptr) {
            cb->onError(Error::CANCELED, 0);
        }
    } else {
        auto faceAIDLService = mFaceAIDLService.lock();
        if (faceAIDLService != nullptr) {
            faceAIDLService->cancel(mCmdId);
        } else {
            LOG(DEBUG) << "FaceAIDLService is nullptr";
            auto cb = mCallback.lock();
            if (cb != nullptr) {
                cb->onError(Error::CANCELED, 0);
            }
        }
    }

    return ndk::ScopedAStatus::ok();
}

} // namespace aidl::android::hardware::biometrics::face
