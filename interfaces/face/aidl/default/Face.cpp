/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Face.h"
#include "FaceAIDLService.h"
#include "Session.h"
#include "SessionManager.h"

#include <android-base/logging.h>

#undef LOG_TAG
#define LOG_TAG "sec_fr_Face"

namespace aidl::android::hardware::biometrics::face {

Face::Face() {
    LOG(DEBUG) << "Face() constructed";
}

Face::~Face() {
    LOG(DEBUG) << "~Face() destructed";
}

ndk::ScopedAStatus Face::getSensorProps(std::vector<SensorProps>* _aidl_return) {
    LOG(DEBUG) << "Face::getSensorProps()";
    SensorProps props;
    props.commonProps.sensorId = 1;
    props.commonProps.sensorStrength = common::SensorStrength::CONVENIENCE;
    props.commonProps.maxEnrollmentsPerUser = 2;
    props.commonProps.componentInfo = {
        common::ComponentInfo{"faceSensor", "", "", "", ""}
    };
    props.sensorType = FaceSensorType::RGB;
    props.halControlsPreview = true;
    props.previewDisplayId = 0;
    props.enrollPreviewWidth = 640;
    props.enrollPreviewHeight = 480;
    props.enrollTranslationX = 0.0f;
    props.enrollTranslationY = 0.0f;
    props.enrollPreviewScale = 0.0f;
    props.supportsDetectInteraction = false;

    _aidl_return->push_back(props);
    LOG(DEBUG) << "Face::getSensorProps() " << props.toString();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Face::createSession(
    int32_t sensorId, int32_t userId,
    const std::shared_ptr<ISessionCallback>& cb,
    std::shared_ptr<ISession>* _aidl_return) {
    LOG(DEBUG) << "Face::createSession()";
    if (cb == nullptr) {
        LOG(ERROR) << "ISessionCallback is nullptr";
        return ndk::ScopedAStatus::ok();
    }
    if (_aidl_return == nullptr) {
        LOG(ERROR) << "ISession is nullptr";
        cb->onError(Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }

    ::android::SessionManager* sm = ::android::SessionManager::getInstance();
    sm->clearSessionBinder();
    if (sm->isAliveClient()) {
        LOG(ERROR) << "Client is already exist";
        LOG(ERROR) << "SessionManager isn't ready";
        cb->onError(Error::UNABLE_TO_PROCESS, 0);
        return ndk::ScopedAStatus::ok();
    }

    LOG(DEBUG) << "Face::initSession()";
    auto session = ndk::SharedRefBase::make<Session>(sensorId, userId);
    if (session == nullptr) {
        LOG(ERROR) << "Session is nullptr";
    } else {
        auto faceAIDLService = std::make_shared<::android::FaceAIDLService>();
        if (session->init(faceAIDLService, cb) == 0) {
            if (sm->addClient(faceAIDLService) != 0) {
                LOG(ERROR) << "Fail to addClient";
            } else {
                *_aidl_return = session;
            }
        } else {
            LOG(ERROR) << "Fail to initialize Session";
        }
    }

    if (*_aidl_return == nullptr) {
        LOG(ERROR) << "Fail to initSession";
        cb->onError(Error::UNABLE_TO_PROCESS, 0);
    }

    return ndk::ScopedAStatus::ok();
}

} // namespace aidl::android::hardware::biometrics::face
