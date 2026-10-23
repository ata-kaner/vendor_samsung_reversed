/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/samsung/hardware/biometrics/face/BnSehSession.h>
#include <memory>
#include <vector>

namespace android {
class FaceAIDLService;
}

namespace aidl::vendor::samsung::hardware::biometrics::face {

class SehSession : public BnSehSession {
public:
    SehSession();
    virtual ~SehSession();

    int init(const std::shared_ptr<::android::FaceAIDLService>& service);

    ndk::ScopedAStatus authenticateExtension(
        int64_t operationId, int32_t sensorId,
        const std::vector<uint8_t>& extra,
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) override;
    ndk::ScopedAStatus getWrappedData(std::vector<uint8_t>* return_val) override;
    ndk::ScopedAStatus loadTA(int32_t* return_val) override;
    ndk::ScopedAStatus unloadTA(int32_t* return_val) override;
    ndk::ScopedAStatus isTAUnloaded(bool* return_val) override;
    ndk::ScopedAStatus pause(int32_t* return_val) override;
    ndk::ScopedAStatus resume(int32_t* return_val) override;
    ndk::ScopedAStatus getFaceTagList(std::vector<SehFaceTagID>* return_val) override;
    ndk::ScopedAStatus getFaceTag(SehFaceTagID tagId, SehFaceTag* return_val) override;
    ndk::ScopedAStatus setFaceTag(
        SehFaceTagID tagId, const std::vector<uint8_t>& data, int32_t* return_val) override;
    ndk::ScopedAStatus setRotation(int32_t rotation, int32_t* return_val) override;
    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus authenticateForIssuance(
        int64_t operationId, int32_t sensorId,
        const std::vector<uint8_t>& extra, bool isIssuance,
        const std::optional<aidl::android::hardware::common::NativeHandle>& previewSurface,
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) override;
    ndk::ScopedAStatus getWrappedDataFromMemory(
        aidl::android::hardware::common::Ashmem* return_val) override;
    ndk::ScopedAStatus getTaInfo(std::string* return_val) override;
    ndk::ScopedAStatus authenticateForIssuanceEx(
        int64_t operationId, int32_t sensorId,
        const std::vector<uint8_t>& extra, bool isIssuance,
        const std::optional<aidl::android::hardware::NativeWindow>& previewSurface,
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) override;

private:
    std::shared_ptr<::android::FaceAIDLService> mFaceAIDLService;
    bool isServiceReady();
};

} // namespace aidl::vendor::samsung::hardware::biometrics::face
