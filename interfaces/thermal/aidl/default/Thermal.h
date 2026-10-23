//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <aidl/android/hardware/thermal/BnThermal.h>
#include <aidl/android/hardware/thermal/IThermalChangedCallback.h>
#include <aidl/android/hardware/thermal/Temperature.h>
#include <aidl/android/hardware/thermal/TemperatureThreshold.h>
#include <aidl/android/hardware/thermal/TemperatureType.h>
#include <android/binder_auto_utils.h>

#include "ThermalCore.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {

using ::vendor::samsung::hardware::thermal::ThermalCore;

class Thermal : public BnThermal {
  public:
    explicit Thermal(std::shared_ptr<ThermalCore> core);
    ~Thermal() override;

    ndk::ScopedAStatus getCoolingDevices(std::vector<CoolingDevice>* out) override;
    ndk::ScopedAStatus getCoolingDevicesWithType(CoolingType type,
                                                std::vector<CoolingDevice>* out) override;
    ndk::ScopedAStatus getTemperatures(std::vector<Temperature>* out) override;
    ndk::ScopedAStatus getTemperaturesWithType(TemperatureType type,
                                               std::vector<Temperature>* out) override;
    ndk::ScopedAStatus getTemperatureThresholds(
            std::vector<TemperatureThreshold>* out) override;
    ndk::ScopedAStatus getTemperatureThresholdsWithType(
            TemperatureType type, std::vector<TemperatureThreshold>* out) override;
    ndk::ScopedAStatus registerThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback>& callback) override;
    ndk::ScopedAStatus registerThermalChangedCallbackWithType(
            const std::shared_ptr<IThermalChangedCallback>& callback,
            TemperatureType type) override;
    ndk::ScopedAStatus unregisterThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback>& callback) override;

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;

    void update_throttling_status();
    void unregisterThermalChangedCallbackByCookie(void* cookie);

  private:
    std::shared_ptr<ThermalCore> mThermalCore;
    mutable std::mutex mCallbackMutex;

    std::map<std::string, Temperature> mTrackedTemperatures;
    std::map<void*, std::shared_ptr<IThermalChangedCallback>> mCallbacks;
    ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;

    static void onBinderDied(void* cookie);
};

}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
