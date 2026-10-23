/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "HwApi.h"
#include "Vibrator.h"
#include "Vibrator_vendor.h"

#include <android-base/logging.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::android::hardware::vibrator::HwApi;
using aidl::android::hardware::vibrator::Vibrator;
namespace SecVibrator = aidl::vendor::samsung::hardware::vibrator;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto hwApi = std::make_shared<HwApi>();
    auto vibrator = ndk::SharedRefBase::make<Vibrator>(hwApi);
    auto secVibrator = ndk::SharedRefBase::make<SecVibrator::Vibrator>(hwApi);

    AIBinder_setExtension(vibrator->asBinder().get(), secVibrator->asBinder().get());

    const std::string instance = std::string(Vibrator::descriptor) + "/default";
    binder_status_t status =
            AServiceManager_addService(vibrator->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    LOG(INFO) << "Samsung Vibrator HAL service started (" << instance << ")";

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // Should not reach
}
