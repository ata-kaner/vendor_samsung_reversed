//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include "ThermalCore.h"

#include <android/log.h>
#include <cutils/trace.h>
#include <unistd.h>
#include <sstream>

#define LOG_TAG "thermal_core"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

static int64_t elapsedRealtime() {
    timespec ts{};
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

ThermalCore::ThermalCore()
    : mScenarioHint(0),
      mPeriods{10000, 10000, 10000},
      mLastUpdateTime(0) {
    LOGI("init thermal core");

    mWeightSetter = std::make_unique<WeightSetter>();
    mBootControl = std::make_unique<BootControl>();

    std::map<SehScenarioHint, Json::Value> scenarios;
    jsonRootMapFromScenarios(scenarios);

    if (scenarios.find(SehScenarioHint::NORMAL) != scenarios.end()) {
        const auto& normalRoot = scenarios[SehScenarioHint::NORMAL];
        mPeriods[0] = readPeriod(normalRoot);

        if (scenarios.find(SehScenarioHint::HEAVY) != scenarios.end()) {
            mPeriods[1] = readPeriod(scenarios[SehScenarioHint::HEAVY]);
        }

        // 1. Physical thermistors
        auto phys = parsePhysicalThermistors(normalRoot, &mTypes);
        if (phys) {
            mThermistorGroups.push_back(std::move(phys));
        }

        // 2. Virtual thermistors
        auto virt = parseVirtualThermistors(normalRoot, &mTypes);
        if (virt) {
            mThermistorGroups.push_back(std::move(virt));
        }

        // 3. LRP thermistors
        auto lrps = parseLrps(scenarios, mWeightSetter.get(), &mTypes);
        if (lrps) {
            mThermistorGroups.push_back(std::move(lrps));
        }

        // 4. Heatmap front
        auto hmFront = parseHeatmap(scenarios, "front", mWeightSetter.get(), &mTypes);
        if (hmFront) {
            mThermistorGroups.push_back(std::move(hmFront));
        }

        // 5. Heatmap rear
        auto hmRear = parseHeatmap(scenarios, "rear", mWeightSetter.get(), &mTypes);
        if (hmRear) {
            mThermistorGroups.push_back(std::move(hmRear));
        }

        // 6. Aggregation thermistors
        auto agg = parseAggregation(normalRoot, &mTypes);
        if (agg) {
            mThermistorGroups.push_back(std::move(agg));
        }

        // Initial reading
        {
            std::lock_guard<std::mutex> lock(mUpdateMutex);
            mLastUpdateTime = elapsedRealtime();
            for (auto& group : mThermistorGroups) {
                group->get_temperatures(mTemperatures);
            }
        }

        LOGI("init thermal core complete");
    } else {
        LOGE("Failed to initialize thermistor group: missing NORMAL config");
    }
}

ThermalCore::~ThermalCore() {
    mThermistorGroups.clear();
}

void ThermalCore::start() {
    if (mBootControl && mBootControl->is_done()) {
        mBootControl.reset();
        LOGI("!@initSehThermal:  Monitoring started.");
        temp_update_poll();
    } else {
        LOGI("!@initSehThermal:  BootControl is started.");
        boot_control_poll();
    }
}

SehTemperatureType ThermalCore::get_type(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    auto it = mTypes.find(name);
    if (it != mTypes.end()) {
        return it->second;
    }
    return SehTemperatureType::UNKNOWN;
}

int ThermalCore::get_temperature(const std::string& name) {
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    auto itDebug = mDebugTemperatures.find(name);
    if (itDebug != mDebugTemperatures.end()) {
        return itDebug->second;
    }
    auto it = mTemperatures.find(name);
    if (it != mTemperatures.end()) {
        return it->second;
    }
    return -999;
}

std::vector<std::string> ThermalCore::get_all_name() const {
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    std::vector<std::string> names;
    names.reserve(mTypes.size());
    for (const auto& pair : mTypes) {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> ThermalCore::get_name_by_type(int type) const {
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    std::vector<std::string> names;
    auto targetType = static_cast<SehTemperatureType>(type);
    for (const auto& pair : mTypes) {
        if (pair.second == targetType) {
            names.push_back(pair.first);
        }
    }
    return names;
}

void ThermalCore::set_debug_temperature(const std::string& name, int temp) {
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    if (temp == -999) {
        mDebugTemperatures.erase(name);
    } else {
        mDebugTemperatures[name] = temp;
    }
}

void ThermalCore::ensure_latest() {
    int64_t now = elapsedRealtime();
    if (now - mLastUpdateTime > 15000) {
        LOGE("need_to_update: %lld", static_cast<long long>(now - mLastUpdateTime));
        std::lock_guard<std::mutex> lock(mUpdateMutex);
        now = elapsedRealtime();
        if (now - mLastUpdateTime < 1901) {
            LOGE("skip_to_update: %lld", static_cast<long long>(now - mLastUpdateTime));
        } else {
            mLastUpdateTime = now;
            for (auto& group : mThermistorGroups) {
                group->get_temperatures(mTemperatures);
            }
            for (const auto& pair : mDebugTemperatures) {
                mTemperatures[pair.first] = pair.second;
            }
        }
    }
}

void ThermalCore::listen(std::function<void()> listener) {
    std::lock_guard<std::mutex> lock(mListenerMutex);
    mListeners.push_back(std::move(listener));
}

void ThermalCore::set_scenario_hint(int hint) {
    mScenarioHint = hint;
    if (mWeightSetter) {
        mWeightSetter->set_scenario(hint);
    }
}

int ThermalCore::get_scenario_hint() const {
    return mScenarioHint;
}

void ThermalCore::temp_update_poll() {
    if (mWeightSetter) {
        mWeightSetter->refresh();
    }

    {
        std::lock_guard<std::mutex> lock(mUpdateMutex);
        int64_t now = elapsedRealtime();
        if (now - mLastUpdateTime < 1901) {
            LOGE("skip_to_update: %lld", static_cast<long long>(now - mLastUpdateTime));
        } else {
            mLastUpdateTime = now;
            for (auto& group : mThermistorGroups) {
                group->get_temperatures(mTemperatures);
            }
            for (const auto& pair : mDebugTemperatures) {
                mTemperatures[pair.first] = pair.second;
            }
        }
    }

    // Atrace counter traces
    int skinTemp = get_temperature("SKIN");
    int apTemp = get_temperature("AP");
    int paTemp = get_temperature("PA");
    atrace_int(ATRACE_TAG_ALWAYS, "SKIN", skinTemp);
    atrace_int(ATRACE_TAG_ALWAYS, "AP", apTemp);
    atrace_int(ATRACE_TAG_ALWAYS, "PA", paTemp);

    // Notify listeners
    {
        std::lock_guard<std::mutex> lock(mListenerMutex);
        for (const auto& listener : mListeners) {
            if (listener) {
                listener();
            }
        }
    }

    // Schedule next poll
    int period = mPeriods[mScenarioHint >= 0 && mScenarioHint < 3 ? mScenarioHint : 0];
    mIntervalTimer.set_callback([this]() { temp_update_poll(); });
    mIntervalTimer.start(period);
}

void ThermalCore::boot_control_poll() {
    if (mBootControl && mBootControl->is_done()) {
        mBootControl.reset();
        LOGI("!@initSehThermal:  Monitoring started.");
        temp_update_poll();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mUpdateMutex);
        int64_t now = elapsedRealtime();
        if (now - mLastUpdateTime >= 1901) {
            mLastUpdateTime = now;
            for (auto& group : mThermistorGroups) {
                group->get_temperatures(mTemperatures);
            }
            for (const auto& pair : mDebugTemperatures) {
                mTemperatures[pair.first] = pair.second;
            }
        }
    }

    int skinTemp = get_temperature("SKIN");
    if (mBootControl) {
        mBootControl->control(skinTemp);
    }

    {
        std::lock_guard<std::mutex> lock(mListenerMutex);
        for (const auto& listener : mListeners) {
            if (listener) {
                listener();
            }
        }
    }

    // Boot control polls every 5000 ms
    mIntervalTimer.set_callback([this]() { boot_control_poll(); });
    mIntervalTimer.start(5000);
}

void ThermalCore::dump(int fd) const {
    std::stringstream ss;
    ss << "=== Samsung Thermal Service Dump ===" << "\n";
    ss << "Scenario Hint: " << mScenarioHint << "\n";
    if (mWeightSetter) {
        std::string wsDump;
        mWeightSetter->dump(&wsDump);
        ss << wsDump;
    }

    ss << "Temperatures:" << "\n";
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    for (const auto& pair : mTemperatures) {
        auto itDebug = mDebugTemperatures.find(pair.first);
        ss << "  " << pair.first << ": " << (static_cast<float>(pair.second) / 10.0f) << " C";
        if (itDebug != mDebugTemperatures.end()) {
            ss << " (override: " << (static_cast<float>(itDebug->second) / 10.0f) << " C)";
        }
        ss << "\n";
    }

    std::string str = ss.str();
    write(fd, str.data(), str.size());
}

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
