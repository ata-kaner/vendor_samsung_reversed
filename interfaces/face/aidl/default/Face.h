/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/face/BnFace.h>
#include <vector>

namespace aidl::android::hardware::biometrics::face {

class Face : public BnFace {
public:
    Face();
    virtual ~Face();

    ndk::ScopedAStatus getSensorProps(std::vector<SensorProps>* _aidl_return) override;
    ndk::ScopedAStatus createSession(
        int32_t sensorId, int32_t userId,
        const std::shared_ptr<ISessionCallback>& cb,
        std::shared_ptr<ISession>* _aidl_return) override;
};

} // namespace aidl::android::hardware::biometrics::face
