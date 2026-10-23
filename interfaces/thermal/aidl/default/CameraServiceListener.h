//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <mutex>
#include <set>
#include <string>
#include <memory>

#include <android/binder_auto_utils.h>

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

class CameraServiceListener {
  public:
    virtual ~CameraServiceListener() = default;
    virtual bool is_camera_active() const = 0;
};

class CameraServiceListenerImpl : public CameraServiceListener {
  public:
    CameraServiceListenerImpl() = default;
    ~CameraServiceListenerImpl() override = default;

    bool is_camera_active() const override {
        std::lock_guard<std::mutex> lock(mMutex);
        return !mActiveCameras.empty();
    }

    void onCameraDeviceStateChanged(const std::string& cameraId, int facing, int newCameraState,
                                    const std::string& clientName, int apiLevel, int userId) {
        (void)facing;
        (void)clientName;
        (void)apiLevel;
        (void)userId;
        std::lock_guard<std::mutex> lock(mMutex);
        // State 3 corresponds to CAMERA_STATE_CLOSED
        if (newCameraState == 3) {
            mActiveCameras.erase(cameraId);
        } else {
            mActiveCameras.insert(cameraId);
        }
    }

    ndk::ScopedAStatus onPhysicalCameraStatusChanged(int status, const std::string& cameraId,
                                                     const std::string& physicalCameraId) {
        (void)status;
        (void)cameraId;
        (void)physicalCameraId;
        return ndk::ScopedAStatus::ok();
    }

  private:
    mutable std::mutex mMutex;
    std::set<std::string> mActiveCameras;
};

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
