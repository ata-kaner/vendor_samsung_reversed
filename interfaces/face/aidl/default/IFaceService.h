/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/face/EnrollmentStageConfig.h>
#include <aidl/android/hardware/biometrics/face/EnrollmentType.h>
#include <aidl/android/hardware/biometrics/face/Feature.h>
#include <aidl/android/hardware/biometrics/face/ISessionCallback.h>
#include <aidl/android/hardware/common/Ashmem.h>
#include <aidl/vendor/samsung/hardware/biometrics/face/SehFaceTagID.h>

#include <hardware/hw_auth_token.h>
#include <system/window.h>

#include <future>
#include <memory>
#include <string>
#include <vector>

namespace android {

class IFaceService {
public:
    virtual ~IFaceService() = default;

    virtual int setActiveUser(int userId, int sensorId) = 0;
    virtual void setNotifyCallback(const std::shared_ptr<aidl::android::hardware::biometrics::face::ISessionCallback>& cb) = 0;
    virtual void generateChallenge() = 0;
    virtual void revokeChallenge(uint64_t challenge) = 0;
    virtual void getEnrollmentConfig(
        aidl::android::hardware::biometrics::face::EnrollmentType type,
        std::vector<aidl::android::hardware::biometrics::face::EnrollmentStageConfig>& configs) = 0;
    virtual uint32_t enroll(
        std::promise<void>&& promise,
        const hw_auth_token_t& hat,
        aidl::android::hardware::biometrics::face::EnrollmentType type,
        const std::vector<aidl::android::hardware::biometrics::face::Feature>& features,
        ANativeWindow* window) = 0;
    virtual uint32_t authenticate(
        std::promise<void>&& promise,
        uint64_t operationId,
        uint32_t sensorId,
        const std::vector<uint8_t>& extra) = 0;
    virtual uint32_t authenticateForIssuance(
        std::promise<void>&& promise,
        uint64_t operationId,
        uint32_t sensorId,
        const std::vector<uint8_t>& extra,
        bool isIssuance,
        ANativeWindow* window) = 0;
    virtual uint32_t detectInteraction(std::promise<void>&& promise) = 0;
    virtual void enumerateEnrollments() = 0;
    virtual void removeEnrollments(const std::vector<int>& enrollmentIds) = 0;
    virtual void getFeature() = 0;
    virtual void setFeature(
        const hw_auth_token_t& hat,
        aidl::android::hardware::biometrics::face::Feature feature,
        bool enabled) = 0;
    virtual void getAuthenticatorId() = 0;
    virtual void invalidateAuthenticatorId() = 0;
    virtual void resetLockout(const hw_auth_token_t& hat) = 0;
    virtual void sessionClose() = 0;
    virtual void cancel(uint32_t cmdId) = 0;
    virtual void getWrappedData(std::vector<uint8_t>& out) = 0;
    virtual void getWrappedDataFromMemory(aidl::android::hardware::common::Ashmem& out) = 0;
    virtual int loadTA() = 0;
    virtual int unloadTA() = 0;
    virtual bool isUnLoadTA() = 0;
    virtual int pause() = 0;
    virtual int resume() = 0;
    virtual int setOrientation(uint32_t rotation) = 0;
    virtual void getFaceTagList(
        std::vector<aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID>& out) = 0;
    virtual void getFaceTag(
        aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID tagId,
        int32_t& dataType,
        std::vector<uint8_t>& out) = 0;
    virtual int setFaceTag(
        aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID tagId,
        const std::vector<uint8_t>& data) = 0;
    virtual std::string getTaInfo() = 0;
};

} // namespace android
