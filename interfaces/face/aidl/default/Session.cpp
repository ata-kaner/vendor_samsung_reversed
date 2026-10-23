/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Session.h"
#include "FaceAIDLService.h"

#include <android-base/logging.h>

#undef LOG_TAG
#define LOG_TAG "Session"

namespace aidl::android::hardware::biometrics::face {

Session::Session(int sensorId, int userId)
    : mSensorId(sensorId), mUserId(userId) {
    LOG(DEBUG) << "Session() constructed";
}

Session::~Session() {
    LOG(DEBUG) << "~Session() destructed";
}

int Session::init(const std::shared_ptr<::android::FaceAIDLService>& service,
                  const std::shared_ptr<ISessionCallback>& cb) {
    LOG(DEBUG) << "Session::init";
    if (service == nullptr) {
        LOG(ERROR) << "service is nullptr";
        return -4;
    }
    mFaceAIDLService = service;
    return mFaceAIDLService->initSession(mSensorId, mUserId, cb);
}

bool Session::isServiceReady() {
    if (mFaceAIDLService == nullptr) {
        LOG(ERROR) << "FaceAIDLService is nullptr";
        return false;
    }
    return true;
}

ndk::ScopedAStatus Session::generateChallenge() {
    LOG(DEBUG) << "Session::generateChallenge";
    if (isServiceReady()) {
        return mFaceAIDLService->generateChallenge();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::revokeChallenge(int64_t challenge) {
    LOG(DEBUG) << "Session::revokeChallenge";
    if (isServiceReady()) {
        return mFaceAIDLService->revokeChallenge(challenge);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::getEnrollmentConfig(
    EnrollmentType enrollmentType,
    std::vector<EnrollmentStageConfig>* return_val) {
    LOG(DEBUG) << "Session::getEnrollmentConfig";
    if (isServiceReady()) {
        return mFaceAIDLService->getEnrollmentConfig(enrollmentType, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enroll(
    const keymaster::HardwareAuthToken& hat,
    EnrollmentType enrollmentType,
    const std::vector<Feature>& toEnroll,
    const std::optional<common::NativeHandle>& previewSurface,
    std::shared_ptr<common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "Session::enroll";
    if (isServiceReady()) {
        return mFaceAIDLService->enroll(hat, enrollmentType, toEnroll, previewSurface, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::authenticate(
    int64_t operationId,
    std::shared_ptr<common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "Session::authenticate";
    if (isServiceReady()) {
        return mFaceAIDLService->authenticate(operationId, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::detectInteraction(
    std::shared_ptr<common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "Session::detectInteraction";
    if (isServiceReady()) {
        return mFaceAIDLService->detectInteraction(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enumerateEnrollments() {
    LOG(DEBUG) << "Session::enumerateEnrollments";
    if (isServiceReady()) {
        return mFaceAIDLService->enumerateEnrollments();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::removeEnrollments(const std::vector<int32_t>& enrollmentIds) {
    LOG(DEBUG) << "Session::removeEnrollments";
    if (isServiceReady()) {
        return mFaceAIDLService->removeEnrollments(enrollmentIds);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::getFeatures() {
    LOG(DEBUG) << "Session::getFeatures";
    if (isServiceReady()) {
        return mFaceAIDLService->getFeatures();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::setFeature(
    const keymaster::HardwareAuthToken& hat,
    Feature feature,
    bool enabled) {
    LOG(DEBUG) << "Session::setFeature";
    if (isServiceReady()) {
        return mFaceAIDLService->setFeature(hat, feature, enabled);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::getAuthenticatorId() {
    LOG(DEBUG) << "Session::getAuthenticatorId";
    if (isServiceReady()) {
        return mFaceAIDLService->getAuthenticatorId();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::invalidateAuthenticatorId() {
    LOG(DEBUG) << "Session::invalidateAuthenticatorId";
    if (isServiceReady()) {
        return mFaceAIDLService->invalidateAuthenticatorId();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::resetLockout(const keymaster::HardwareAuthToken& hat) {
    LOG(DEBUG) << "Session::resetLockout";
    if (isServiceReady()) {
        return mFaceAIDLService->resetLockout(hat);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::close() {
    LOG(DEBUG) << "Session::close";
    if (isServiceReady()) {
        return mFaceAIDLService->close();
    }
    return ndk::ScopedAStatus::ok();
}

} // namespace aidl::android::hardware::biometrics::face
