/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SessionManager.h"
#include "FaceAIDLService.h"

#include <android-base/logging.h>

#undef LOG_TAG
#define LOG_TAG "sec_fr_SessionManager"

namespace android {

SessionManager::SessionManager() {
    LOG(DEBUG) << "SessionManager() <" << this << "> constructed";
}

SessionManager* SessionManager::getInstance() {
    static SessionManager* sInstance = nullptr;
    static std::once_flag sOnce;
    std::call_once(sOnce, []() {
        LOG(DEBUG) << "SessionManager::call_once";
        sInstance = new SessionManager();
    });
    return sInstance;
}

int SessionManager::addClient(const std::shared_ptr<FaceAIDLService>& client) {
    LOG(DEBUG) << "SessionManager::addClient";
    std::lock_guard<std::mutex> lock(mLock);
    if (client == nullptr) {
        LOG(ERROR) << "FaceAIDLService is nullptr";
        return -4;
    }
    mClient = client;
    mIsRemovedClient = false;
    return 0;
}

void SessionManager::removeClient() {
    LOG(DEBUG) << "SessionManager::removeClient";
    std::lock_guard<std::mutex> lock(mLock);
    mClient.reset();
    mIsRemovedClient = true;
}

bool SessionManager::isAliveClient() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mClient.expired()) {
        LOG(DEBUG) << "Client is expired (mIsRemovedClient=" << mIsRemovedClient << ")";
        return false;
    }
    return true;
}

int SessionManager::setSessionBinder(const ::ndk::SpAIBinder& session, const ::ndk::SpAIBinder& sehSession) {
    LOG(DEBUG) << "SessionManager::setSessionBinder";
    std::lock_guard<std::mutex> lock(mLock);
    if (session.get() == nullptr || sehSession.get() == nullptr) {
        LOG(ERROR) << "session or sehSession is nullptr";
        return -4;
    }
    mSessionBinder = session;
    mSehSessionBinder = sehSession;
    return 0;
}

void SessionManager::clearSessionBinder() {
    std::lock_guard<std::mutex> lock(mLock);
    mSessionBinder = nullptr;
    mSehSessionBinder = nullptr;
}

} // namespace android
