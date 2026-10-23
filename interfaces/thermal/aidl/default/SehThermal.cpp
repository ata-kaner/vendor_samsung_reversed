//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include "SehThermal.h"

#include <android-base/logging.h>
#include <android/binder_ibinder.h>

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

void SehThermal::onBinderDied(void* cookie) {
    auto self = reinterpret_cast<SehThermal*>(cookie);
    if (self) {
        self->unregisterThermalChangedCallbackByCookie(cookie);
    }
}

SehThermal::SehThermal(std::shared_ptr<ThermalCore> core)
    : mThermalCore(std::move(core)),
      mDeathRecipient(AIBinder_DeathRecipient_new(onBinderDied)) {
    LOG(INFO) << "SehThermal init!";

    if (mThermalCore) {
        mThermalCore->listen([this]() { notify_to_listeners(); });
    }
}

SehThermal::~SehThermal() = default;

void SehThermal::unregisterThermalChangedCallbackByCookie(void* cookie) {
    LOG(INFO) << "unregisterThermalChangedCallbackByCookie cookie: " << cookie;
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    mCallbacks.erase(cookie);
}

void SehThermal::name_to_temperatures(const std::vector<std::string>& names,
                                      std::vector<SehTemperature>* out) {
    if (!out || !mThermalCore) {
        return;
    }
    out->clear();
    out->reserve(names.size());

    for (const auto& name : names) {
        SehTemperature t;
        t.name = name;
        t.type = static_cast<int32_t>(mThermalCore->get_type(name));
        int raw = mThermalCore->get_temperature(name);
        t.value = (raw > -300) ? (static_cast<float>(raw) / 10.0f) : -99.9f;
        out->push_back(t);
    }
}

void SehThermal::notify_to_listeners() {
    if (!mThermalCore) {
        return;
    }

    std::vector<std::string> names = mThermalCore->get_all_name();
    std::vector<SehTemperature> temps;
    name_to_temperatures(names, &temps);

    std::lock_guard<std::mutex> lock(mCallbackMutex);
    for (const auto& pair : mCallbacks) {
        if (pair.second) {
            pair.second->notifyTemperatures(temps);
        }
    }
}

ndk::ScopedAStatus SehThermal::getTemperatures(std::vector<SehTemperature>* out) {
    LOG(DEBUG) << "getTemperatures";
    if (!out) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!mThermalCore) {
        return ndk::ScopedAStatus::ok();
    }
    mThermalCore->ensure_latest();
    std::vector<std::string> names = mThermalCore->get_all_name();
    name_to_temperatures(names, out);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehThermal::getTemperaturesWithType(int32_t type,
                                                      std::vector<SehTemperature>* out) {
    LOG(DEBUG) << "getTemperaturesWithType SehTemperatureType: " << type;
    if (!out) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!mThermalCore) {
        return ndk::ScopedAStatus::ok();
    }
    mThermalCore->ensure_latest();
    std::vector<std::string> names = mThermalCore->get_name_by_type(type);
    name_to_temperatures(names, out);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehThermal::registerThermalChangedCallback(
        const std::shared_ptr<ISehThermalChangedCallback>& callback) {
    LOG(DEBUG) << "registerThermalChangedCallback ISehThermalChangedCallback: " << callback.get();
    if (!callback) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT, "Invalid nullptr callback");
    }

    void* cookie = callback.get();
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (mCallbacks.find(cookie) != mCallbacks.end()) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT, "Callback already registered");
    }

    mCallbacks[cookie] = callback;
    binder_status_t status = AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient.get(), cookie);
    if (status != STATUS_OK) {
        LOG(WARNING) << "fail to link death recipient";
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehThermal::unregisterThermalChangedCallback(
        const std::shared_ptr<ISehThermalChangedCallback>& callback) {
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

ndk::ScopedAStatus SehThermal::setScenarioHint(int32_t hint) {
    LOG(DEBUG) << "setScenarioHint hint: " << hint;
    if (mThermalCore) {
        mThermalCore->set_scenario_hint(hint);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehThermal::getScenarioHint(int32_t* out) {
    LOG(DEBUG) << "getScenarioHint";
    if (!out) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (mThermalCore) {
        *out = mThermalCore->get_scenario_hint();
    } else {
        *out = 0;
    }
    return ndk::ScopedAStatus::ok();
}

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
