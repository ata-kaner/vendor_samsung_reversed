/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SehFace.h"
#include "FaceAIDLService.h"
#include "SehSession.h"
#include "Session.h"
#include "SessionManager.h"

#include <android-base/logging.h>
#include <unistd.h>

#undef LOG_TAG
#define LOG_TAG "sec_fr_SehFace"

namespace aidl::vendor::samsung::hardware::biometrics::face {

SehFace::SehFace() {
    LOG(DEBUG) << "SehFace() constructed";
}

SehFace::~SehFace() {
    LOG(DEBUG) << "~SehFace() destructed";
}

ndk::ScopedAStatus SehFace::getServicePid(int32_t* _aidl_return) {
    LOG(DEBUG) << "SehFace::getServicePid";
    if (_aidl_return == nullptr) {
        LOG(ERROR) << "pid is nullptr";
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = getpid();
    LOG(DEBUG) << "FaceServicePID : " << *_aidl_return;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehFace::createSession(
    int32_t sensorId, int32_t userId,
    const std::shared_ptr<aidl::android::hardware::biometrics::face::ISessionCallback>& cb,
    std::shared_ptr<ISehSession>* _aidl_return) {
    LOG(DEBUG) << "SehFace::createSession";
    if (cb == nullptr) {
        LOG(ERROR) << "ISessionCallback is NULL !!!!";
        return ndk::ScopedAStatus::ok();
    }
    if (_aidl_return == nullptr) {
        LOG(ERROR) << "ISehSession is nullptr";
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }

    ::android::SessionManager* sm = ::android::SessionManager::getInstance();
    sm->clearSessionBinder();
    if (sm->isAliveClient()) {
        LOG(ERROR) << "Client is already exist";
        LOG(ERROR) << "SessionManager isn't ready";
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }

    auto faceAIDLService = std::make_shared<::android::FaceAIDLService>();

    LOG(DEBUG) << "SehFace::initSession()";
    auto session = ndk::SharedRefBase::make<aidl::android::hardware::biometrics::face::Session>(sensorId, userId);
    if (session == nullptr) {
        LOG(ERROR) << "Session is nullptr";
        LOG(ERROR) << "Fail to initSession";
        sm->clearSessionBinder();
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }
    if (session->init(faceAIDLService, cb) != 0) {
        LOG(ERROR) << "Fail to initialize Session";
        LOG(ERROR) << "Fail to initSession";
        sm->clearSessionBinder();
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }

    LOG(DEBUG) << "SehFace::initSehSession()";
    auto sehSession = ndk::SharedRefBase::make<SehSession>();
    if (sehSession == nullptr) {
        LOG(ERROR) << "SehSession is nullptr";
        LOG(ERROR) << "Fail to initSehSession";
        sm->clearSessionBinder();
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }
    if (sehSession->init(faceAIDLService) != 0) {
        LOG(ERROR) << "Fail to initialize SehSession";
        LOG(ERROR) << "Fail to initSehSession";
        sm->clearSessionBinder();
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }

    LOG(DEBUG) << "SehFace::setSession()";
    auto sessionBinder = session->asBinder();
    auto sehSessionBinder = sehSession->asBinder();
    if (sm->setSessionBinder(sessionBinder, sehSessionBinder) != 0) {
        LOG(ERROR) << "Fail to setSessionBinder";
        sm->clearSessionBinder();
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }
    if (sm->addClient(faceAIDLService) != 0) {
        LOG(ERROR) << "Fail to addClient";
        sm->clearSessionBinder();
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }

    binder_status_t status = AIBinder_setExtension(sehSessionBinder.get(), sessionBinder.get());
    if (status != STATUS_OK) {
        LOG(ERROR) << "AIBinder_setExtension is failed = " << status;
        sm->clearSessionBinder();
        cb->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }

    *_aidl_return = sehSession;
    return ndk::ScopedAStatus::ok();
}

} // namespace aidl::vendor::samsung::hardware::biometrics::face
