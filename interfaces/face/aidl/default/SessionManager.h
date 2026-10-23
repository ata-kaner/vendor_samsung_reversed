/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/binder_auto_utils.h>
#include <memory>
#include <mutex>

namespace android {

class FaceAIDLService;

class SessionManager {
public:
    static SessionManager* getInstance();

    int addClient(const std::shared_ptr<FaceAIDLService>& client);
    void removeClient();
    bool isAliveClient();
    int setSessionBinder(const ::ndk::SpAIBinder& session, const ::ndk::SpAIBinder& sehSession);
    void clearSessionBinder();

private:
    SessionManager();
    ~SessionManager() = default;

    std::mutex mLock;
    std::weak_ptr<FaceAIDLService> mClient;
    ::ndk::SpAIBinder mSessionBinder;
    ::ndk::SpAIBinder mSehSessionBinder;
    bool mIsRemovedClient = false;
};

} // namespace android
