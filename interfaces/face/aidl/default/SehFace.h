/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/samsung/hardware/biometrics/face/BnSehFace.h>

namespace aidl::vendor::samsung::hardware::biometrics::face {

class SehFace : public BnSehFace {
public:
    SehFace();
    virtual ~SehFace();

    ndk::ScopedAStatus getServicePid(int32_t* _aidl_return) override;
    ndk::ScopedAStatus createSession(
        int32_t sensorId, int32_t userId,
        const std::shared_ptr<aidl::android::hardware::biometrics::face::ISessionCallback>& cb,
        std::shared_ptr<ISehSession>* _aidl_return) override;
};

} // namespace aidl::vendor::samsung::hardware::biometrics::face
