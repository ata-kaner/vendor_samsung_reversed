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

#include <aidl/vendor/samsung/hardware/thermal/BnSehThermal.h>
#include <aidl/vendor/samsung/hardware/thermal/ISehThermalChangedCallback.h>
#include <aidl/vendor/samsung/hardware/thermal/SehScenarioHint.h>
#include <aidl/vendor/samsung/hardware/thermal/SehTemperature.h>
#include <aidl/vendor/samsung/hardware/thermal/SehTemperatureType.h>
#include <android/binder_auto_utils.h>

#include "ThermalCore.h"

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

using ::vendor::samsung::hardware::thermal::ThermalCore;

class SehThermal : public BnSehThermal {
  public:
    explicit SehThermal(std::shared_ptr<ThermalCore> core);
    ~SehThermal() override;

    ndk::ScopedAStatus getTemperatures(std::vector<SehTemperature>* out) override;
    ndk::ScopedAStatus getTemperaturesWithType(int32_t type,
                                               std::vector<SehTemperature>* out) override;
    ndk::ScopedAStatus registerThermalChangedCallback(
            const std::shared_ptr<ISehThermalChangedCallback>& callback) override;
    ndk::ScopedAStatus unregisterThermalChangedCallback(
            const std::shared_ptr<ISehThermalChangedCallback>& callback) override;
    ndk::ScopedAStatus setScenarioHint(int32_t hint) override;
    ndk::ScopedAStatus getScenarioHint(int32_t* out) override;

    void notify_to_listeners();
    void unregisterThermalChangedCallbackByCookie(void* cookie);

  private:
    void name_to_temperatures(const std::vector<std::string>& names,
                              std::vector<SehTemperature>* out);

    std::shared_ptr<ThermalCore> mThermalCore;
    mutable std::mutex mCallbackMutex;

    std::map<void*, std::shared_ptr<ISehThermalChangedCallback>> mCallbacks;
    ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;

    static void onBinderDied(void* cookie);
};

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
