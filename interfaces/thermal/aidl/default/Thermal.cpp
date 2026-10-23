//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include "Thermal.h"

#include <android-base/logging.h>
#include <android/binder_ibinder.h>
#include <cmath>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {

using ::aidl::vendor::samsung::hardware::thermal::SehTemperatureType;

static const std::vector<float> kSkinThresholds = {38.0f, 40.0f, 42.0f, 45.0f, 47.0f, 60.0f, 90.0f};

static TemperatureType toAospType(SehTemperatureType type) {
    switch (type) {
        case SehTemperatureType::CPU: return TemperatureType::CPU;
        case SehTemperatureType::GPU: return TemperatureType::GPU;
        case SehTemperatureType::BATTERY: return TemperatureType::BATTERY;
        case SehTemperatureType::SKIN: return TemperatureType::SKIN;
        case SehTemperatureType::USB_PORT: return TemperatureType::USB_PORT;
        case SehTemperatureType::POWER_AMPLIFIER: return TemperatureType::POWER_AMPLIFIER;
        case SehTemperatureType::BCL_VOLTAGE: return TemperatureType::BCL_VOLTAGE;
        case SehTemperatureType::BCL_CURRENT: return TemperatureType::BCL_CURRENT;
        case SehTemperatureType::BCL_PERCENTAGE: return TemperatureType::BCL_PERCENTAGE;
        case SehTemperatureType::NPU: return TemperatureType::NPU;
        case SehTemperatureType::TPU: return TemperatureType::TPU;
        case SehTemperatureType::DISPLAY: return TemperatureType::DISPLAY;
        case SehTemperatureType::MODEM: return TemperatureType::MODEM;
        case SehTemperatureType::SOC: return TemperatureType::SOC;
        default: return TemperatureType::UNKNOWN;
    }
}

void Thermal::onBinderDied(void* cookie) {
    auto self = reinterpret_cast<Thermal*>(cookie);
    if (self) {
        self->unregisterThermalChangedCallbackByCookie(cookie);
    }
}

Thermal::Thermal(std::shared_ptr<ThermalCore> core)
    : mThermalCore(std::move(core)),
      mDeathRecipient(AIBinder_DeathRecipient_new(onBinderDied)) {
    LOG(INFO) << "Thermal init!";

    if (mThermalCore) {
        mThermalCore->listen([this]() { update_throttling_status(); });

        // Initialize tracking for BATTERY (type 2) and SKIN (type 3)
        for (const auto& name : mThermalCore->get_name_by_type(static_cast<int>(SehTemperatureType::BATTERY))) {
            Temperature t;
            t.type = TemperatureType::BATTERY;
            t.name = name;
            t.value = NAN;
            t.throttlingStatus = ThrottlingSeverity::NONE;
            mTrackedTemperatures[name] = t;
        }

        for (const auto& name : mThermalCore->get_name_by_type(static_cast<int>(SehTemperatureType::SKIN))) {
            Temperature t;
            t.type = TemperatureType::SKIN;
            t.name = name;
            t.value = NAN;
            t.throttlingStatus = ThrottlingSeverity::NONE;
            mTrackedTemperatures[name] = t;
        }
    }
}

Thermal::~Thermal() = default;

void Thermal::unregisterThermalChangedCallbackByCookie(void* cookie) {
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    mCallbacks.erase(cookie);
}

void Thermal::update_throttling_status() {
    if (!mThermalCore) {
        return;
    }

    for (auto& pair : mTrackedTemperatures) {
        const std::string& name = pair.first;
        Temperature& t = pair.second;

        int raw = mThermalCore->get_temperature(name);
        if (raw < -300) {
            continue;
        }
        float fval = static_cast<float>(raw) / 10.0f;
        ThrottlingSeverity oldSeverity = t.throttlingStatus;
        ThrottlingSeverity newSeverity = oldSeverity;

        if (t.type == TemperatureType::BATTERY) {
            if (fval >= 85.0f) {
                newSeverity = ThrottlingSeverity::SHUTDOWN;
            } else if (fval >= 55.0f) {
                newSeverity = ThrottlingSeverity::EMERGENCY;
            } else if (fval <= 50.0f) {
                newSeverity = ThrottlingSeverity::NONE;
            }
        } else if (t.type == TemperatureType::SKIN) {
            newSeverity = ThrottlingSeverity::NONE;
            for (int i = static_cast<int>(kSkinThresholds.size()) - 1; i >= 0; --i) {
                if (fval >= kSkinThresholds[i]) {
                    newSeverity = static_cast<ThrottlingSeverity>(i);
                    break;
                }
            }
        }

        if (newSeverity != oldSeverity) {
            t.value = fval;
            t.throttlingStatus = newSeverity;

            std::lock_guard<std::mutex> lock(mCallbackMutex);
            for (const auto& cbPair : mCallbacks) {
                if (cbPair.second) {
                    cbPair.second->notifyThrottling(t);
                }
            }
        }
    }
}

ndk::ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice>* out) {
    if (out) {
        out->clear();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType /*type*/,
                                                     std::vector<CoolingDevice>* out) {
    if (out) {
        out->clear();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperatures(std::vector<Temperature>* out) {
    if (!out) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    out->clear();

    if (!mThermalCore) {
        return ndk::ScopedAStatus::ok();
    }
    mThermalCore->ensure_latest();

    for (const auto& name : mThermalCore->get_all_name()) {
        int raw = mThermalCore->get_temperature(name);
        Temperature t;
        t.type = toAospType(mThermalCore->get_type(name));
        t.name = name;
        t.value = (raw > -300) ? (static_cast<float>(raw) / 10.0f) : NAN;

        auto itTracked = mTrackedTemperatures.find(name);
        t.throttlingStatus = (itTracked != mTrackedTemperatures.end())
                                     ? itTracked->second.throttlingStatus
                                     : ThrottlingSeverity::NONE;

        out->push_back(t);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType type,
                                                   std::vector<Temperature>* out) {
    if (!out) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    out->clear();

    if (!mThermalCore) {
        return ndk::ScopedAStatus::ok();
    }
    mThermalCore->ensure_latest();

    for (const auto& name : mThermalCore->get_all_name()) {
        TemperatureType tType = toAospType(mThermalCore->get_type(name));
        if (tType == type) {
            int raw = mThermalCore->get_temperature(name);
            Temperature t;
            t.type = tType;
            t.name = name;
            t.value = (raw > -300) ? (static_cast<float>(raw) / 10.0f) : NAN;

            auto itTracked = mTrackedTemperatures.find(name);
            t.throttlingStatus = (itTracked != mTrackedTemperatures.end())
                                         ? itTracked->second.throttlingStatus
                                         : ThrottlingSeverity::NONE;

            out->push_back(t);
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperatureThresholds(
        std::vector<TemperatureThreshold>* out) {
    if (!out) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    out->clear();

    if (!mThermalCore) {
        return ndk::ScopedAStatus::ok();
    }

    for (const auto& name : mThermalCore->get_name_by_type(static_cast<int>(SehTemperatureType::SKIN))) {
        TemperatureThreshold th;
        th.type = TemperatureType::SKIN;
        th.name = name;
        th.hotThrottlingThresholds = kSkinThresholds;
        th.coldThrottlingThresholds = std::vector<float>(kSkinThresholds.size(), NAN);
        out->push_back(th);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType type, std::vector<TemperatureThreshold>* out) {
    if (!out) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    out->clear();

    if (type == TemperatureType::SKIN) {
        return getTemperatureThresholds(out);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& callback) {
    if (!callback) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT, "Invalid nullptr callback");
    }

    void* cookie = callback.get();
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (mCallbacks.find(cookie) != mCallbacks.end()) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT, "Callback already registered");
    }

    mCallbacks[cookie] = callback;
    AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient.get(), cookie);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
        const std::shared_ptr<IThermalChangedCallback>& callback,
        TemperatureType /*type*/) {
    return registerThermalChangedCallback(callback);
}

ndk::ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback>& callback) {
    if (!callback) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT, "Invalid nullptr callback");
    }

    void* cookie = callback.get();
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    auto it = mCallbacks.find(cookie);
    if (it == mCallbacks.end()) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT, "Callback not registered");
    }

    AIBinder_unlinkToDeath(callback->asBinder().get(), mDeathRecipient.get(), cookie);
    mCallbacks.erase(it);
    return ndk::ScopedAStatus::ok();
}

binder_status_t Thermal::dump(int fd, const char** args, uint32_t numArgs) {
    if (numArgs >= 3 && std::string(args[0]) == "debug") {
        std::string sensorName = args[1];
        int temp = std::stoi(args[2]);
        if (mThermalCore) {
            mThermalCore->set_debug_temperature(sensorName, temp);
        }
        return STATUS_OK;
    }

    if (mThermalCore) {
        mThermalCore->dump(fd);
    }
    return STATUS_OK;
}

}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
