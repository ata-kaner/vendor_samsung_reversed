/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.samsung.hardware.thermal@1.0-service"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>
#include "SehThermal.h"

using ::android::OK;
using ::android::sp;
using ::android::status_t;

// libhwbinder:
using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;

// Generated HIDL files:
using ::vendor::samsung::hardware::thermal::V1_0::ISehThermal;
using ::vendor::samsung::hardware::thermal::V1_0::implementation::SehThermal;

static int shutdown() {
    LOG(ERROR) << "Thermal Service is shutting down.";
    return 1;
}

int main(int /* argc */, char** /* argv */) {
    status_t status;
    sp<ISehThermal> service = nullptr;

    LOG(INFO) << "SEC Thermal HAL Service 1.0 starting...";

    service = new SehThermal();
    if (service == nullptr) {
        LOG(ERROR) << "Error creating an instance of ThermalHAL. Exiting...";
        return shutdown();
    }

    configureRpcThreadpool(1, true /* callerWillJoin */);

    status = service->registerAsService();
    if (status != OK) {
        LOG(ERROR) << "Could not register service for SEC ThermalHAL (" << status << ")";
        return shutdown();
    }

    LOG(INFO) << "Thermal Service started successfully.";
    joinRpcThreadpool();

    return shutdown();
}
