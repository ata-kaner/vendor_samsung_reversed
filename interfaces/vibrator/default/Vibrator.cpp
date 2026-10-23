/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Vibrator.h"

#include <android-base/logging.h>

#include <chrono>
#include <thread>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

Vibrator::Vibrator(std::shared_ptr<HwApi> hwApi) : mHwApi(std::move(hwApi)) {}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t* _aidl_return) {
    int32_t capabilities = 0;

    if (mHwApi->isAmplitudeControlExist()) {
        capabilities |= static_cast<int32_t>(IVibrator::CAP_AMPLITUDE_CONTROL);
    }
    if (mHwApi->hasFeature(PRIMITIVE_EFFECT_COMPOSE) ||
        mHwApi->isNodeExist(mHwApi->getCpTriggerQueueNode())) {
        capabilities |= static_cast<int32_t>(IVibrator::CAP_COMPOSE_EFFECTS);
    }
    if (mHwApi->hasFeature(CAPABILITY_EXTERNAL_CONTROL)) {
        capabilities |= (static_cast<int32_t>(IVibrator::CAP_ALWAYS_ON_CONTROL) |
                         static_cast<int32_t>(IVibrator::CAP_EXTERNAL_CONTROL));
    }

    LOG(INFO) << "Vibrator reporting capabilities: " << capabilities;
    *_aidl_return = capabilities;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    LOG(INFO) << "Vibrator off";
    mHwApi->do_off();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs,
                                const std::shared_ptr<IVibratorCallback>& callback) {
    LOG(INFO) << "Vibrator on for timeoutMs: " << timeoutMs;
    if (timeoutMs <= 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    mHwApi->do_on(timeoutMs);

    if (callback != nullptr) {
        std::thread([timeoutMs, callback]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
            callback->onComplete();
        }).detach();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength strength,
                                     const std::shared_ptr<IVibratorCallback>& callback,
                                     int32_t* _aidl_return) {
    LOG(INFO) << "Vibrator perform Effect : " << static_cast<int>(effect);

    if (effect != Effect::CLICK && effect != Effect::DOUBLE_CLICK &&
        effect != Effect::TICK && effect != Effect::HEAVY_CLICK) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    if (strength != EffectStrength::LIGHT && strength != EffectStrength::MEDIUM &&
        strength != EffectStrength::STRONG) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    int timeout = 0;
    if (!mHwApi->performEffect(effect, strength, &timeout)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *_aidl_return = timeout;

    if (callback != nullptr) {
        std::thread([timeout, callback]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            callback->onComplete();
        }).detach();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect>* _aidl_return) {
    *_aidl_return = {Effect::CLICK, Effect::DOUBLE_CLICK, Effect::TICK, Effect::HEAVY_CLICK};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
    if (amplitude <= 0.0f || amplitude > 1.0f) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!mHwApi->isAmplitudeControlExist()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    int32_t amp = static_cast<int32_t>(amplitude * 10000.0f);
    mHwApi->setAmplitude(amp);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setExternalControl(bool enabled) {
    if (!mHwApi->hasFeature(CAPABILITY_EXTERNAL_CONTROL)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    mHwApi->writeNode(mHwApi->getExtControlNode(), enabled ? 1 : 0);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t* _aidl_return) {
    *_aidl_return = COMPOSITION_DELAY_MAX_MS;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t* _aidl_return) {
    *_aidl_return = COMPOSITION_SIZE_MAX;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedPrimitives(
        std::vector<CompositePrimitive>* _aidl_return) {
    LOG(INFO) << "Vibrator getSupportedPrimitives ";
    *_aidl_return = {
        CompositePrimitive::NOOP,
        CompositePrimitive::CLICK,
        CompositePrimitive::THUD,
        CompositePrimitive::SPIN,
        CompositePrimitive::QUICK_RISE,
        CompositePrimitive::SLOW_RISE,
        CompositePrimitive::QUICK_FALL,
        CompositePrimitive::LIGHT_TICK,
    };
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getPrimitiveDuration(CompositePrimitive primitive,
                                                  int32_t* _aidl_return) {
    switch (primitive) {
        case CompositePrimitive::NOOP:
            *_aidl_return = 0;
            break;
        case CompositePrimitive::CLICK:
            *_aidl_return = 20;
            break;
        case CompositePrimitive::THUD:
            *_aidl_return = 300;
            break;
        case CompositePrimitive::SPIN:
            *_aidl_return = 130;
            break;
        case CompositePrimitive::QUICK_RISE:
            *_aidl_return = 150;
            break;
        case CompositePrimitive::SLOW_RISE:
            *_aidl_return = 500;
            break;
        case CompositePrimitive::QUICK_FALL:
            *_aidl_return = 100;
            break;
        case CompositePrimitive::LIGHT_TICK:
            *_aidl_return = 20;
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::compose(const std::vector<CompositeEffect>& composite,
                                     const std::shared_ptr<IVibratorCallback>& callback) {
    LOG(INFO) << "Vibrator compose : " << composite.size();

    if (composite.size() > static_cast<size_t>(COMPOSITION_SIZE_MAX)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    std::string sequence;
    int32_t totalDurationMs = 0;

    for (size_t i = 0; i < composite.size(); ++i) {
        const auto& effect = composite[i];
        if (effect.delayMs > COMPOSITION_DELAY_MAX_MS || effect.delayMs < 0) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }

        if (effect.scale < 0.0f || effect.scale > 1.0f) {
            LOG(ERROR) << " e.scale error ";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }

        int32_t primDuration = 0;
        auto status = getPrimitiveDuration(effect.primitive, &primDuration);
        if (!status.isOk()) {
            return status;
        }

        totalDurationMs += effect.delayMs + primDuration;

        if (effect.primitive == CompositePrimitive::NOOP) {
            LOG(DEBUG) << " CompositePrimitive is NOOP ";
            continue;
        }

        if (!sequence.empty()) {
            sequence += ",";
        }
        sequence += std::to_string(static_cast<int>(effect.primitive)) + "." +
                    std::to_string(effect.delayMs);
    }

    if (!mHwApi->writeNodeByStringOnlyForWriting(mHwApi->getCpTriggerQueueNode(), sequence)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    if (callback != nullptr) {
        std::thread([totalDurationMs, callback]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(totalDurationMs));
            callback->onComplete();
        }).detach();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(std::vector<Effect>* _aidl_return) {
    LOG(INFO) << "Vibrator getSupportedAlwaysOnEffects ";
    *_aidl_return = {Effect::CLICK};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::alwaysOnEnable(int32_t /*id*/, Effect /*effect*/,
                                            EffectStrength /*strength*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::alwaysOnDisable(int32_t /*id*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getResonantFrequency(float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getQFactor(float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getFrequencyResolution(float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getFrequencyMinimum(float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getBandwidthAmplitudeMap(std::vector<float>* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getPwlePrimitiveDurationMax(int32_t* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getPwleCompositionSizeMax(int32_t* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getSupportedBraking(std::vector<Braking>* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::composePwle(const std::vector<PrimitivePwle>& /*composite*/,
                                         const std::shared_ptr<IVibratorCallback>& /*callback*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
