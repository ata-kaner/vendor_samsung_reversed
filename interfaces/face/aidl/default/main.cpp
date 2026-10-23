/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Face.h"
#include "SehFace.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <hidl/HidlSupport.h>
#include <signal.h>

#undef LOG_TAG
#define LOG_TAG "vendor.samsung.hardware.biometrics.face-service"

using aidl::android::hardware::biometrics::face::Face;
using aidl::vendor::samsung::hardware::biometrics::face::SehFace;

int main() {
    LOG(DEBUG) << "Face AIDL started";

    signal(SIGPIPE, SIG_IGN);

    android::hardware::configureRpcThreadpool(5, true);
    ABinderProcess_setThreadPoolMaxThreadCount(6);
    ABinderProcess_startThreadPool();

    std::shared_ptr<Face> faceHal = ndk::SharedRefBase::make<Face>();
    std::shared_ptr<SehFace> sehHal = ndk::SharedRefBase::make<SehFace>();

    auto faceBinder = faceHal->asBinder();
    CHECK(STATUS_OK == AIBinder_setExtension(faceBinder.get(), sehHal->asBinder().get()));

    const std::string instance = std::string(Face::descriptor) + "/default";
    CHECK(STATUS_OK == AServiceManager_addService(faceBinder.get(), instance.c_str()));

    ABinderProcess_joinThreadPool();
    return 1;
}
