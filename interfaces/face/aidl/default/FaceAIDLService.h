/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "IFaceService.h"

#include <aidl/android/hardware/biometrics/common/ICancellationSignal.h>
#include <aidl/android/hardware/biometrics/face/EnrollmentStageConfig.h>
#include <aidl/android/hardware/biometrics/face/EnrollmentType.h>
#include <aidl/android/hardware/biometrics/face/Feature.h>
#include <aidl/android/hardware/biometrics/face/ISessionCallback.h>
#include <aidl/android/hardware/common/Ashmem.h>
#include <aidl/android/hardware/common/NativeHandle.h>
#include <aidl/android/hardware/keymaster/HardwareAuthToken.h>
#include <aidl/vendor/samsung/hardware/biometrics/face/SehFaceTag.h>
#include <aidl/vendor/samsung/hardware/biometrics/face/SehFaceTagID.h>

#include <android/binder_auto_utils.h>
#include <android/hardware_buffer.h>
#include <android/native_window.h>
#include <cutils/native_handle.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace android {

class FaceLibLoader {
public:
    static FaceLibLoader* getInstance();
    IFaceService* getFaceService();

private:
    FaceLibLoader() = default;
    int loadLib();

    static inline FaceLibLoader* sInstance = nullptr;
    static inline std::mutex sMutex;

    void* mLibHandle = nullptr;
    int (*mGetPointer)(IFaceService**) = nullptr;
    void (*mDestroy)() = nullptr;
    bool mIsLoaded = false;
    IFaceService* mIFaceAuth = nullptr;
    std::string mLibName = "libFaceService.so";
    std::string mCreateSym = "_ZN7android11FaceService10getPointerEv";
    std::string mDestroySym = "_ZN7android11FaceService7destroyEv";
    std::mutex mLock;
};

class FaceAIDLService : public std::enable_shared_from_this<FaceAIDLService> {
public:
    FaceAIDLService();
    ~FaceAIDLService();

    int initSession(int sensorId, int userId,
                    const std::shared_ptr<aidl::android::hardware::biometrics::face::ISessionCallback>& cb);
    void cancel(uint32_t cmdId);
    ::ndk::ScopedAStatus close();
    static void onClientDeath(void* cookie);

    // Standard AOSP AIDL methods
    ::ndk::ScopedAStatus generateChallenge();
    ::ndk::ScopedAStatus revokeChallenge(int64_t challenge);
    ::ndk::ScopedAStatus getEnrollmentConfig(
        aidl::android::hardware::biometrics::face::EnrollmentType enrollmentType,
        std::vector<aidl::android::hardware::biometrics::face::EnrollmentStageConfig>* return_val);
    ::ndk::ScopedAStatus enroll(
        const aidl::android::hardware::keymaster::HardwareAuthToken& hat,
        aidl::android::hardware::biometrics::face::EnrollmentType enrollmentType,
        const std::vector<aidl::android::hardware::biometrics::face::Feature>& toEnroll,
        const std::optional<aidl::android::hardware::common::NativeHandle>& previewSurface,
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val);
    ::ndk::ScopedAStatus authenticate(
        int64_t operationId,
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val);
    ::ndk::ScopedAStatus detectInteraction(
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val);
    ::ndk::ScopedAStatus enumerateEnrollments();
    ::ndk::ScopedAStatus removeEnrollments(const std::vector<int32_t>& enrollmentIds);
    ::ndk::ScopedAStatus getFeatures();
    ::ndk::ScopedAStatus setFeature(
        const aidl::android::hardware::keymaster::HardwareAuthToken& hat,
        aidl::android::hardware::biometrics::face::Feature feature,
        bool enabled);
    ::ndk::ScopedAStatus getAuthenticatorId();
    ::ndk::ScopedAStatus invalidateAuthenticatorId();
    ::ndk::ScopedAStatus resetLockout(
        const aidl::android::hardware::keymaster::HardwareAuthToken& hat);

    // Samsung SEH extension AIDL methods
    ::ndk::ScopedAStatus authenticateExtension(
        int64_t operationId, int32_t sensorId,
        const std::vector<uint8_t>& extra,
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val);
    ::ndk::ScopedAStatus getWrappedData(std::vector<uint8_t>* return_val);
    ::ndk::ScopedAStatus loadTA(int32_t* return_val);
    ::ndk::ScopedAStatus unloadTA(int32_t* return_val);
    ::ndk::ScopedAStatus isTAUnloaded(bool* return_val);
    ::ndk::ScopedAStatus pause(int32_t* return_val);
    ::ndk::ScopedAStatus resume(int32_t* return_val);
    ::ndk::ScopedAStatus getFaceTagList(
        std::vector<aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID>* return_val);
    ::ndk::ScopedAStatus getFaceTag(
        aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID tagId,
        aidl::vendor::samsung::hardware::biometrics::face::SehFaceTag* return_val);
    ::ndk::ScopedAStatus setFaceTag(
        aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID tagId,
        const std::vector<uint8_t>& data, int32_t* return_val);
    ::ndk::ScopedAStatus setRotation(int32_t rotation, int32_t* return_val);
    ::ndk::ScopedAStatus authenticateForIssuance(
        int64_t operationId, int32_t sensorId,
        const std::vector<uint8_t>& extra, bool isIssuance,
        const std::optional<aidl::android::hardware::common::NativeHandle>& previewSurface,
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val);
    ::ndk::ScopedAStatus getWrappedDataFromMemory(
        aidl::android::hardware::common::Ashmem* return_val);
    ::ndk::ScopedAStatus getTaInfo(std::string* return_val);
    ::ndk::ScopedAStatus authenticateForIssuanceEx(
        int64_t operationId, int32_t sensorId,
        const std::vector<uint8_t>& extra, bool isIssuance,
        const std::optional<aidl::android::hardware::NativeWindow>& previewSurface,
        std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val);

private:
    int setupCommunicate();
    static IFaceService* getFaceService();
    static void convertHAT(
        const aidl::android::hardware::keymaster::HardwareAuthToken& hat,
        hw_auth_token_t* outHat);
    static ANativeWindow* getANativeWindow(const native_handle_t* handle);

    template <typename T>
    bool isReturnValid(T* retVal);

    std::shared_ptr<aidl::android::hardware::biometrics::face::ISessionCallback> mCallback;
    std::mutex mLock;
    AIBinder_DeathRecipient* mDeathRecipient = nullptr;
};

} // namespace android
