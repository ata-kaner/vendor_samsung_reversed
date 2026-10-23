//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "BootControl.h"
#include "IntervalTimer.h"
#include "mercury.h"

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

class ThermalCore {
  public:
    ThermalCore();
    ~ThermalCore();

    void start();

    SehTemperatureType get_type(const std::string& name) const;
    int get_temperature(const std::string& name);
    std::vector<std::string> get_all_name() const;
    std::vector<std::string> get_name_by_type(int type) const;
    void set_debug_temperature(const std::string& name, int temp);
    void ensure_latest();

    void listen(std::function<void()> listener);
    void set_scenario_hint(int hint);
    int get_scenario_hint() const;

    void dump(int fd) const;

  private:
    void temp_update_poll();
    void boot_control_poll();

    mutable std::mutex mUpdateMutex;
    mutable std::mutex mListenerMutex;

    std::map<std::string, SehTemperatureType> mTypes;
    std::map<std::string, int> mTemperatures;
    std::map<std::string, int> mDebugTemperatures;

    std::vector<std::unique_ptr<ThermistorGroup>> mThermistorGroups;
    std::unique_ptr<WeightSetter> mWeightSetter;
    std::unique_ptr<BootControl> mBootControl;

    int mScenarioHint;
    int mPeriods[3];
    int64_t mLastUpdateTime;

    IntervalTimer mIntervalTimer;
    std::vector<std::function<void()>> mListeners;
};

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
