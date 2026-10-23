/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SehSession.h"
#include "FaceAIDLService.h"

#include <android-base/logging.h>

#undef LOG_TAG
#define LOG_TAG "SehSession"

namespace aidl::vendor::samsung::hardware::biometrics::face {

SehSession::SehSession() {
    LOG(DEBUG) << "SehSession() constructed";
}

SehSession::~SehSession() {
    LOG(DEBUG) << "~SehSession() destructed";
}

int SehSession::init(const std::shared_ptr<::android::FaceAIDLService>& service) {
    LOG(DEBUG) << "SehSession::init";
    if (service == nullptr) {
        LOG(ERROR) << "service is nullptr";
        return -4;
    }
    mFaceAIDLService = service;
    return 0;
}

bool SehSession::isServiceReady() {
    if (mFaceAIDLService == nullptr) {
        LOG(ERROR) << "FaceAIDLService is nullptr";
        return false;
    }
    return true;
}

ndk::ScopedAStatus SehSession::authenticateExtension(
    int64_t operationId, int32_t sensorId,
    const std::vector<uint8_t>& extra,
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "SehSession::authenticateExtension";
    if (isServiceReady()) {
        return mFaceAIDLService->authenticateExtension(operationId, sensorId, extra, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::getWrappedData(std::vector<uint8_t>* return_val) {
    LOG(DEBUG) << "SehSession::getWrappedData";
    if (isServiceReady()) {
        return mFaceAIDLService->getWrappedData(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::loadTA(int32_t* return_val) {
    LOG(DEBUG) << "SehSession::loadTA";
    if (isServiceReady()) {
        return mFaceAIDLService->loadTA(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::unloadTA(int32_t* return_val) {
    LOG(DEBUG) << "SehSession::unloadTA";
    if (isServiceReady()) {
        return mFaceAIDLService->unloadTA(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::isTAUnloaded(bool* return_val) {
    LOG(DEBUG) << "SehSession::isTAUnloaded";
    if (isServiceReady()) {
        return mFaceAIDLService->isTAUnloaded(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::pause(int32_t* return_val) {
    LOG(DEBUG) << "SehSession::pause";
    if (isServiceReady()) {
        return mFaceAIDLService->pause(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::resume(int32_t* return_val) {
    LOG(DEBUG) << "SehSession::resume";
    if (isServiceReady()) {
        return mFaceAIDLService->resume(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::getFaceTagList(std::vector<SehFaceTagID>* return_val) {
    LOG(DEBUG) << "SehSession::getFaceTagList";
    if (isServiceReady()) {
        return mFaceAIDLService->getFaceTagList(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::getFaceTag(SehFaceTagID tagId, SehFaceTag* return_val) {
    LOG(DEBUG) << "SehSession::getFaceTag";
    if (isServiceReady()) {
        return mFaceAIDLService->getFaceTag(tagId, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::setFaceTag(
    SehFaceTagID tagId, const std::vector<uint8_t>& data, int32_t* return_val) {
    LOG(DEBUG) << "SehSession::setFaceTag";
    if (isServiceReady()) {
        return mFaceAIDLService->setFaceTag(tagId, data, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::setRotation(int32_t rotation, int32_t* return_val) {
    LOG(DEBUG) << "SehSession::setRotation";
    if (isServiceReady()) {
        return mFaceAIDLService->setRotation(rotation, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::close() {
    LOG(DEBUG) << "SehSession::close";
    if (isServiceReady()) {
        return mFaceAIDLService->close();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::authenticateForIssuance(
    int64_t operationId, int32_t sensorId,
    const std::vector<uint8_t>& extra, bool isIssuance,
    const std::optional<aidl::android::hardware::common::NativeHandle>& previewSurface,
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "SehSession::authenticateForIssuance";
    if (isServiceReady()) {
        return mFaceAIDLService->authenticateForIssuance(
            operationId, sensorId, extra, isIssuance, previewSurface, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::getWrappedDataFromMemory(
    aidl::android::hardware::common::Ashmem* return_val) {
    LOG(DEBUG) << "SehSession::getWrappedDataFromMemory";
    if (isServiceReady()) {
        return mFaceAIDLService->getWrappedDataFromMemory(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::getTaInfo(std::string* return_val) {
    LOG(DEBUG) << "SehSession::getTaInfo";
    if (isServiceReady()) {
        return mFaceAIDLService->getTaInfo(return_val);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehSession::authenticateForIssuanceEx(
    int64_t operationId, int32_t sensorId,
    const std::vector<uint8_t>& extra, bool isIssuance,
    const std::optional<aidl::android::hardware::NativeWindow>& previewSurface,
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "SehSession::authenticateForIssuanceEx";
    if (isServiceReady()) {
        return mFaceAIDLService->authenticateForIssuanceEx(
            operationId, sensorId, extra, isIssuance, previewSurface, return_val);
    }
    return ndk::ScopedAStatus::ok();
}

} // namespace aidl::vendor::samsung::hardware::biometrics::face
