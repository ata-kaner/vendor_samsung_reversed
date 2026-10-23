/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SehThermal-HAL"

#include "SehThermal.h"

#include <android-base/logging.h>
#include <cmath>
#include <fstream>
#include <sstream>

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {

const char* ISehThermal::descriptor = "vendor.samsung.hardware.thermal@1.0::ISehThermal";

::android::sp<ISehThermal> ISehThermal::getService(const std::string& serviceName, bool getStub) {
    return ::android::hardware::details::getServiceInternal<implementation::SehThermal, ISehThermal>(
            serviceName, true, getStub);
}

::android::status_t ISehThermal::registerAsService(const std::string& serviceName) {
    return ::android::hardware::details::registerAsServiceInternal(this, serviceName);
}

namespace implementation {

using ::android::hardware::interfacesEqual;

// Static skin throttling thresholds embedded in the Samsung binary
static const TemperatureThreshold kSkinThreshold = {
        .type = TemperatureType::SKIN,
        .name = "SKIN",
        .hotThrottlingThresholds = {{36.0f, 38.0f, 40.0f, 42.0f, 46.0f, 55.0f, 85.0f}},
        .coldThrottlingThresholds = {{34.0f, 36.0f, 38.0f, 40.0f, 44.0f, 50.0f, 60.0f}},
        .vrThrottlingThreshold = NAN,
};

SehThermal::SehThermal() {
    LOG(INFO) << "Initializing Samsung SehThermal HAL...";
    mTemperatureProvider = std::make_unique<TemperatureProvider>(
            [this](hidl_vec<SehTemperature>& sehTemps, hidl_vec<Temperature_2_0>& v2Temps) {
                notifyToListeners(sehTemps, v2Temps);
            });
}

SehThermal::~SehThermal() {
    LOG(INFO) << "Destructing Samsung SehThermal HAL...";
}

// -----------------------------------------------------------------------------
// Methods from ::android::hardware::thermal::V1_0::IThermal
// -----------------------------------------------------------------------------

Return<void> SehThermal::getTemperatures(getTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";

    std::vector<Temperature_1_0> temperatures;
    if (mTemperatureProvider) {
        const auto& currentTemps = mTemperatureProvider->getTemperatures();
        const auto& types = mTemperatureProvider->getTypes();

        for (const auto& pair : currentTemps) {
            if (pair.second <= -300) continue;

            const std::string& name = pair.first;
            float val = static_cast<float>(pair.second) / 10.0f;

            SehTemperatureType sehType = SehTemperatureType::SKIN;
            auto itType = types.find(name);
            if (itType != types.end()) {
                sehType = itType->second;
            }

            Temperature_1_0 t;
            t.type = static_cast<::android::hardware::thermal::V1_0::TemperatureType>(sehType);
            t.name = name;
            t.currentValue = val;
            t.throttlingThreshold = 42.0f;
            t.shutdownThreshold = 85.0f;
            t.vrThrottlingThreshold = NAN;

            temperatures.push_back(t);
        }
    }

    _hidl_cb(status, temperatures);
    return Void();
}

Return<void> SehThermal::getCpuUsages(getCpuUsages_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";

    std::vector<CpuUsage> cpuUsages;
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        std::string line;
        while (std::getline(statFile, line)) {
            if (line.compare(0, 3, "cpu") == 0 && line.size() > 3 && isdigit(line[3])) {
                std::istringstream ss(line);
                std::string cpuName;
                uint64_t user = 0, nice = 0, system = 0, idle = 0;
                uint64_t iowait = 0, irq = 0, softirq = 0, steal = 0;

                ss >> cpuName >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

                uint64_t active = user + nice + system + irq + softirq + steal;
                uint64_t total = active + idle + iowait;

                CpuUsage usage;
                usage.name = cpuName;
                usage.active = active;
                usage.total = total;
                usage.isOnline = true;

                cpuUsages.push_back(usage);
            }
        }
    }

    _hidl_cb(status, cpuUsages);
    return Void();
}

Return<void> SehThermal::getCoolingDevices(getCoolingDevices_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";

    // HIDL 1.0 getCoolingDevices returns an empty vector in this binary
    std::vector<CoolingDevice_1_0> devices;
    _hidl_cb(status, devices);
    return Void();
}

// -----------------------------------------------------------------------------
// Methods from ::android::hardware::thermal::V2_0::IThermal
// -----------------------------------------------------------------------------

Return<void> SehThermal::getCurrentTemperatures(bool filterType, TemperatureType type,
                                               getCurrentTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";

    std::vector<Temperature_2_0> temperatures;
    if (mTemperatureProvider) {
        const auto& currentTemps = mTemperatureProvider->getTemperatures();
        const auto& types = mTemperatureProvider->getTypes();

        for (const auto& pair : currentTemps) {
            if (pair.second <= -300) continue;

            const std::string& name = pair.first;
            float val = static_cast<float>(pair.second) / 10.0f;

            SehTemperatureType sehType = SehTemperatureType::SKIN;
            auto itType = types.find(name);
            if (itType != types.end()) {
                sehType = itType->second;
            }

            TemperatureType v2Type = static_cast<TemperatureType>(sehType);
            if (filterType && v2Type != type) {
                continue;
            }

            Temperature_2_0 t;
            t.type = v2Type;
            t.name = name;
            t.value = val;
            t.throttlingStatus = getThrottlingSeverity(v2Type, val, name);

            temperatures.push_back(t);
        }
    }

    _hidl_cb(status, temperatures);
    return Void();
}

Return<void> SehThermal::getTemperatureThresholds(bool filterType, TemperatureType type,
                                                 getTemperatureThresholds_cb _hidl_cb) {
    ThermalStatus status;

    if (filterType && type != TemperatureType::SKIN) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "no threshold data for temperature type ";
        _hidl_cb(status, {});
        return Void();
    }

    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";

    std::vector<TemperatureThreshold> thresholds = {kSkinThreshold};
    _hidl_cb(status, thresholds);
    return Void();
}

Return<void> SehThermal::registerThermalChangedCallback(
        const sp<IThermalChangedCallback>& callback, bool filterType, TemperatureType type,
        registerThermalChangedCallback_cb _hidl_cb) {
    ThermalStatus status;

    if (callback == nullptr) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Invalid nullptr callback";
        _hidl_cb(status);
        return Void();
    }

    std::lock_guard<std::mutex> lock(mCallbackLock);
    for (const auto& cb : mCallbacks) {
        if (interfacesEqual(cb.callback, callback)) {
            status.code = ThermalStatusCode::FAILURE;
            status.debugMessage = "Callback interface registered already";
            _hidl_cb(status);
            return Void();
        }
    }

    mCallbacks.emplace_back(callback, filterType, type);
    callback->linkToDeath(this, 0);

    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";
    _hidl_cb(status);
    return Void();
}

Return<void> SehThermal::unregisterThermalChangedCallback(
        const sp<IThermalChangedCallback>& callback,
        unregisterThermalChangedCallback_cb _hidl_cb) {
    ThermalStatus status;

    if (callback == nullptr) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Invalid nullptr callback";
        _hidl_cb(status);
        return Void();
    }

    std::lock_guard<std::mutex> lock(mCallbackLock);
    bool found = false;
    for (auto it = mCallbacks.begin(); it != mCallbacks.end(); ++it) {
        if (interfacesEqual(it->callback, callback)) {
            callback->unlinkToDeath(this);
            mCallbacks.erase(it);
            found = true;
            break;
        }
    }

    if (found) {
        status.code = ThermalStatusCode::SUCCESS;
        status.debugMessage = "";
    } else {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Callback interface not registered";
    }

    _hidl_cb(status);
    return Void();
}

Return<void> SehThermal::getCurrentCoolingDevices(bool filterType, CoolingType type,
                                                 getCurrentCoolingDevices_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";

    std::vector<CoolingDevice_2_0> devices;

    // 1. CPU Cooling Device
    if (!filterType || type == CoolingType::CPU) {
        std::ifstream freqFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        int64_t curFreq = 0;
        if (freqFile.is_open()) {
            freqFile >> curFreq;
        }

        CoolingDevice_2_0 cpuDev;
        cpuDev.type = CoolingType::CPU;
        cpuDev.name = "cpu";
        cpuDev.value = curFreq;
        devices.push_back(cpuDev);
    }

    // 2. Battery SIOP Level Cooling Device
    if (!filterType || type == CoolingType::BATTERY) {
        std::ifstream siopFile("/sys/class/power_supply/battery/siop_level");
        int64_t siopLevel = 100;
        if (siopFile.is_open()) {
            siopFile >> siopLevel;
        }

        CoolingDevice_2_0 batDev;
        batDev.type = CoolingType::BATTERY;
        batDev.name = "battery";
        batDev.value = siopLevel;
        devices.push_back(batDev);
    }

    _hidl_cb(status, devices);
    return Void();
}

// -----------------------------------------------------------------------------
// Methods from ::vendor::samsung::hardware::thermal::V1_0::ISehThermal
// -----------------------------------------------------------------------------

Return<void> SehThermal::sehGetTemperatures(sehGetTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";

    std::vector<SehTemperature> sehTemps;
    if (mTemperatureProvider) {
        const auto& currentTemps = mTemperatureProvider->getTemperatures();
        const auto& types = mTemperatureProvider->getTypes();

        for (const auto& pair : currentTemps) {
            if (pair.second <= -300) continue;

            const std::string& name = pair.first;
            float val = static_cast<float>(pair.second) / 10.0f;

            SehTemperatureType sehType = SehTemperatureType::SKIN;
            auto itType = types.find(name);
            if (itType != types.end()) {
                sehType = itType->second;
            }

            SehTemperature t;
            t.type = sehType;
            t.name = name;
            t.currentValue = val;
            sehTemps.push_back(t);
        }
    }

    _hidl_cb(status, sehTemps);
    return Void();
}

Return<void> SehThermal::sehGetTypeTemperatures(SehTemperatureType type,
                                              sehGetTypeTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";

    std::vector<SehTemperature> filtered;
    if (mTemperatureProvider) {
        const auto& currentTemps = mTemperatureProvider->getTemperatures();
        const auto& types = mTemperatureProvider->getTypes();

        for (const auto& pair : currentTemps) {
            if (pair.second <= -300) continue;

            const std::string& name = pair.first;
            float val = static_cast<float>(pair.second) / 10.0f;

            SehTemperatureType sehType = SehTemperatureType::SKIN;
            auto itType = types.find(name);
            if (itType != types.end()) {
                sehType = itType->second;
            }

            if (sehType == type) {
                SehTemperature t;
                t.type = sehType;
                t.name = name;
                t.currentValue = val;
                filtered.push_back(t);
            }
        }
    }

    _hidl_cb(status, filtered);
    return Void();
}

Return<void> SehThermal::sehRegisterThermalChangedCallback(
        const sp<ISehThermalChangedCallback>& callback,
        sehRegisterThermalChangedCallback_cb _hidl_cb) {
    ThermalStatus status;

    if (callback == nullptr) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Invalid nullptr callback";
        _hidl_cb(status);
        return Void();
    }

    std::lock_guard<std::mutex> lock(mCallbackLock);
    for (const auto& cb : mSehCallbacks) {
        if (interfacesEqual(cb, callback)) {
            status.code = ThermalStatusCode::FAILURE;
            status.debugMessage = "Callback interface registered already";
            _hidl_cb(status);
            return Void();
        }
    }

    mSehCallbacks.push_back(callback);
    callback->linkToDeath(this, 1);

    status.code = ThermalStatusCode::SUCCESS;
    status.debugMessage = "";
    _hidl_cb(status);
    return Void();
}

Return<void> SehThermal::sehUnregisterThermalChangedCallback(
        const sp<ISehThermalChangedCallback>& callback,
        sehUnregisterThermalChangedCallback_cb _hidl_cb) {
    ThermalStatus status;

    if (callback == nullptr) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Invalid nullptr callback";
        _hidl_cb(status);
        return Void();
    }

    std::lock_guard<std::mutex> lock(mCallbackLock);
    bool found = false;
    for (auto it = mSehCallbacks.begin(); it != mSehCallbacks.end(); ++it) {
        if (interfacesEqual(*it, callback)) {
            callback->unlinkToDeath(this);
            mSehCallbacks.erase(it);
            found = true;
            break;
        }
    }

    if (found) {
        status.code = ThermalStatusCode::SUCCESS;
        status.debugMessage = "";
    } else {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Callback interface not registered";
    }

    _hidl_cb(status);
    return Void();
}

void SehThermal::serviceDied(uint64_t /* cookie */, const wp<IBase>& who) {
    std::lock_guard<std::mutex> lock(mCallbackLock);

    // Clean up IThermalChangedCallback
    for (auto it = mCallbacks.begin(); it != mCallbacks.end();) {
        if (interfacesEqual(it->callback, who)) {
            it = mCallbacks.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up ISehThermalChangedCallback
    for (auto it = mSehCallbacks.begin(); it != mSehCallbacks.end();) {
        if (interfacesEqual(*it, who)) {
            it = mSehCallbacks.erase(it);
        } else {
            ++it;
        }
    }
}

void SehThermal::notifyToListeners(hidl_vec<SehTemperature>& sehTemps,
                                   hidl_vec<Temperature_2_0>& v2Temps) {
    std::lock_guard<std::mutex> lock(mCallbackLock);

    // Notify Samsung vendor callback listeners
    for (const auto& cb : mSehCallbacks) {
        if (cb != nullptr) {
            cb->notifyTemperatures(sehTemps);
        }
    }

    // Notify standard HIDL 2.0 callback listeners
    for (const auto& setting : mCallbacks) {
        if (setting.callback == nullptr) continue;

        for (const auto& t : v2Temps) {
            if (!setting.filterType || setting.type == t.type) {
                setting.callback->notifyThrottling(t);
            }
        }
    }
}

ThrottlingSeverity SehThermal::getThrottlingSeverity(TemperatureType type, float value,
                                                    const std::string& name) {
    if (name == "BAT" || name.find("SKIN") != std::string::npos || type == TemperatureType::SKIN) {
        const auto& hot = kSkinThreshold.hotThrottlingThresholds;
        if (value >= hot[static_cast<size_t>(ThrottlingSeverity::SHUTDOWN)]) {
            return ThrottlingSeverity::SHUTDOWN;
        }
        if (value >= hot[static_cast<size_t>(ThrottlingSeverity::EMERGENCY)]) {
            return ThrottlingSeverity::EMERGENCY;
        }
        if (value >= hot[static_cast<size_t>(ThrottlingSeverity::CRITICAL)]) {
            return ThrottlingSeverity::CRITICAL;
        }
        if (value >= hot[static_cast<size_t>(ThrottlingSeverity::SEVERE)]) {
            return ThrottlingSeverity::SEVERE;
        }
        if (value >= hot[static_cast<size_t>(ThrottlingSeverity::MODERATE)]) {
            return ThrottlingSeverity::MODERATE;
        }
        if (value >= hot[static_cast<size_t>(ThrottlingSeverity::LIGHT)]) {
            return ThrottlingSeverity::LIGHT;
        }
    }
    return ThrottlingSeverity::NONE;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
