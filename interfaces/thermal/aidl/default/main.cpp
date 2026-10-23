//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include <android-base/logging.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "SehThermal.h"
#include "Thermal.h"
#include "ThermalCore.h"

using ::aidl::android::hardware::thermal::IThermal;
using ::aidl::android::hardware::thermal::Thermal;
using ::aidl::vendor::samsung::hardware::thermal::ISehThermal;
using ::aidl::vendor::samsung::hardware::thermal::SehThermal;
using ::vendor::samsung::hardware::thermal::ThermalCore;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(3);

    auto thermalCore = std::make_shared<ThermalCore>();
    auto thermal = ndk::SharedRefBase::make<Thermal>(thermalCore);
    auto sehThermal = ndk::SharedRefBase::make<SehThermal>(thermalCore);

    ndk::SpAIBinder thermalBinder = thermal->asBinder();
    ndk::SpAIBinder sehThermalBinder = sehThermal->asBinder();

    binder_status_t status = AIBinder_setExtension(thermalBinder.get(), sehThermalBinder.get());
    CHECK(status == STATUS_OK) << "Failed to set ISehThermal extension on IThermal";

    std::string instance = std::string(IThermal::descriptor) + "/default";
    status = AServiceManager_addService(thermalBinder.get(), instance.c_str());
    CHECK(status == STATUS_OK) << "Failed to register IThermal service as " << instance;

    thermalCore->start();

    ABinderProcess_joinThreadPool();
    return EXIT_SUCCESS;
}
