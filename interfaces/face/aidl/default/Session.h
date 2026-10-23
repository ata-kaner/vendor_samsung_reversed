/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/face/BnSession.h>
#include <aidl/android/hardware/biometrics/face/ISessionCallback.h>

#include <memory>
#include <vector>

namespace android {
class FaceAIDLService;
}

namespace aidl::android::hardware::biometrics::face {

class Session : public BnSession {
public:
    Session(int sensorId, int userId);
    virtual ~Session();

    int init(const std::shared_ptr<::android::FaceAIDLService>& service,
             const std::shared_ptr<ISessionCallback>& cb);

    ndk::ScopedAStatus generateChallenge() override;
    ndk::ScopedAStatus revokeChallenge(int64_t challenge) override;
    ndk::ScopedAStatus getEnrollmentConfig(
        EnrollmentType enrollmentType,
        std::vector<EnrollmentStageConfig>* return_val) override;
    ndk::ScopedAStatus enroll(
        const keymaster::HardwareAuthToken& hat,
        EnrollmentType enrollmentType,
        const std::vector<Feature>& toEnroll,
        const std::optional<common::NativeHandle>& previewSurface,
        std::shared_ptr<common::ICancellationSignal>* return_val) override;
    ndk::ScopedAStatus authenticate(
        int64_t operationId,
        std::shared_ptr<common::ICancellationSignal>* return_val) override;
    ndk::ScopedAStatus detectInteraction(
        std::shared_ptr<common::ICancellationSignal>* return_val) override;
    ndk::ScopedAStatus enumerateEnrollments() override;
    ndk::ScopedAStatus removeEnrollments(
        const std::vector<int32_t>& enrollmentIds) override;
    ndk::ScopedAStatus getFeatures() override;
    ndk::ScopedAStatus setFeature(
        const keymaster::HardwareAuthToken& hat,
        Feature feature,
        bool enabled) override;
    ndk::ScopedAStatus getAuthenticatorId() override;
    ndk::ScopedAStatus invalidateAuthenticatorId() override;
    ndk::ScopedAStatus resetLockout(
        const keymaster::HardwareAuthToken& hat) override;
    ndk::ScopedAStatus close() override;

private:
    int mSensorId;
    int mUserId;
    std::shared_ptr<::android::FaceAIDLService> mFaceAIDLService;
    bool isServiceReady();
};

} // namespace aidl::android::hardware::biometrics::face
