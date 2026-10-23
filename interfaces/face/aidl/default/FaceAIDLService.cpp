/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FaceAIDLService.h"
#include "CancellationSignal.h"
#include "SessionManager.h"

#include <aidl/android/hardware/biometrics/face/Error.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <android-base/logging.h>
#include <android/hardware/graphics/bufferqueue/1.0/IGraphicBufferProducer.h>
#include <android/hidl/base/1.0/IBase.h>
#include <android/hidl/token/1.0/ITokenManager.h>
#include <android/native_window.h>
#include <android/native_window_aidl.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <gui/Surface.h>
#include <gui/bufferqueue/1.0/H2BGraphicBufferProducer.h>
#include <hidl/HidlSupport.h>
#include <hidl/TokenOrData.h>

#undef LOG_TAG
#define LOG_TAG "sec_fr_FaceAIDLService"

namespace android {

// Helper hton64
static inline uint64_t hton64(uint64_t val) {
    return (((uint64_t)htonl(val & 0xFFFFFFFF)) << 32) | htonl(val >> 32);
}

// FaceLibLoader implementation
FaceLibLoader* FaceLibLoader::getInstance() {
    std::lock_guard<std::mutex> lock(sMutex);
    if (sInstance == nullptr) {
        sInstance = new FaceLibLoader();
    }
    return sInstance;
}

int FaceLibLoader::loadLib() {
    LOG(INFO) << "FaceLibLoader::loadLib";
    LOG(INFO) << "FaceLibLoader::openLib";
    if (mLibHandle == nullptr) {
        mLibHandle = dlopen(mLibName.c_str(), RTLD_NOW);
        if (mLibHandle == nullptr) {
            const char* err = dlerror();
            LOG(ERROR) << "open fail error = " << (err ? err : "unknown");
            LOG(ERROR) << "openLib fail (" << mLibName << ")";
            return -1;
        }
    } else {
        LOG(INFO) << "lib already is opened";
    }

    LOG(INFO) << "FaceLibLoader::findSymbol";
    mGetPointer = reinterpret_cast<int (*)(IFaceService**)>(dlsym(mLibHandle, mCreateSym.c_str()));
    LOG(INFO) << "findSymbol result = " << (mGetPointer != nullptr) << " (" << mCreateSym << ")";

    if (mGetPointer != nullptr) {
        LOG(INFO) << "FaceLibLoader::findSymbol";
        mDestroy = reinterpret_cast<void (*)()>(dlsym(mLibHandle, mDestroySym.c_str()));
        LOG(INFO) << "findSymbol result = " << (mDestroy != nullptr) << " (" << mDestroySym << ")";
        if (mDestroy != nullptr) {
            mIsLoaded = true;
            return 0;
        }
    }

    LOG(ERROR) << "not found symbol create(" << mCreateSym << ") destroy(" << mDestroySym << ")";
    return -1;
}

IFaceService* FaceLibLoader::getFaceService() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mIFaceAuth == nullptr) {
        if (!mIsLoaded) {
            if (loadLib() != 0) {
                LOG(ERROR) << "loadLib is failed";
                return nullptr;
            }
        }
        if (mGetPointer != nullptr) {
            mGetPointer(&mIFaceAuth);
            if (mIFaceAuth == nullptr) {
                LOG(ERROR) << "mIFaceAuth is NULL";
            }
        }
    }
    return mIFaceAuth;
}

// FaceAIDLService implementation
FaceAIDLService::FaceAIDLService() {
    LOG(DEBUG) << "FaceAIDLService() constructed";
}

FaceAIDLService::~FaceAIDLService() {
    LOG(DEBUG) << "~FaceAIDLService() destructed";
}

IFaceService* FaceAIDLService::getFaceService() {
    FaceLibLoader* loader = FaceLibLoader::getInstance();
    if (loader == nullptr) {
        LOG(ERROR) << "FaceLibLoader is NULL";
        return nullptr;
    }
    return loader->getFaceService();
}

int FaceAIDLService::setupCommunicate() {
    SessionManager* sm = SessionManager::getInstance();
    sm->clearSessionBinder();
    if (!sm->isAliveClient()) {
        LOG(ERROR) << "setupCommunicate failed : client isn't alive";
        return -8;
    }
    return 0;
}

template <typename T>
bool FaceAIDLService::isReturnValid(T* retVal) {
    if (retVal == nullptr) {
        LOG(ERROR) << "FaceAIDLService::isReturnValid - ReturnValue is nullptr";
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
        return false;
    }
    return true;
}

void FaceAIDLService::convertHAT(
    const aidl::android::hardware::keymaster::HardwareAuthToken& hat,
    hw_auth_token_t* outHat) {
    LOG(DEBUG) << "FaceAIDLService::convertHAT";
    memset(outHat, 0, sizeof(hw_auth_token_t));

    outHat->version = 0;
    outHat->challenge = hat.challenge;
    outHat->user_id = hat.userId;
    outHat->authenticator_id = hat.authenticatorId;
    outHat->authenticator_type = htonl(static_cast<uint32_t>(hat.authenticatorType));
    outHat->timestamp = hton64(hat.timestamp.milliSeconds);

    size_t macSize = hat.mac.size() > 32 ? 32 : hat.mac.size();
    memcpy(outHat->hmac, hat.mac.data(), macSize);
}

ANativeWindow* FaceAIDLService::getANativeWindow(const native_handle_t* handle) {
    if (handle == nullptr) {
        LOG(ERROR) << "getANativeWindow - handle is nullptr";
        return nullptr;
    }
    LOG(DEBUG) << "getANativeWindow IN";

    if (handle->numFds != 0 || handle->numInts < 2) {
        return nullptr;
    }

    hardware::hidl_vec<uint8_t> halToken;
    halToken.setToExternal(
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&handle->data[1])),
        handle->data[0]);

    sp<android::hidl::base::V1_0::IBase> base = android::retrieveHalInterface(halToken);
    if (base == nullptr) {
        LOG(ERROR) << "retrieveHalInterface is failed";
        return nullptr;
    }

    sp<hardware::graphics::bufferqueue::V1_0::IGraphicBufferProducer> producer =
        hardware::graphics::bufferqueue::V1_0::IGraphicBufferProducer::castFrom(base);
    if (producer == nullptr) {
        LOG(ERROR) << "HGraphicBufferProducer is failed";
        return nullptr;
    }

    LOG(DEBUG) << "convertNativeHandleToHGBP is success~";
    sp<IGraphicBufferProducer> bp = new H2BGraphicBufferProducer(producer);
    return new Surface(bp, true);
}

void FaceAIDLService::onClientDeath(void* cookie) {
    LOG(DEBUG) << "FaceService has died";
    if (cookie != nullptr) {
        reinterpret_cast<FaceAIDLService*>(cookie)->close();
    }
}

int FaceAIDLService::initSession(
    int sensorId, int userId,
    const std::shared_ptr<aidl::android::hardware::biometrics::face::ISessionCallback>& cb) {
    LOG(DEBUG) << "FaceAIDLService::initSession";
    std::lock_guard<std::mutex> lock(mLock);

    ::ndk::SpAIBinder binder = cb->asBinder();
    if (binder.get() == nullptr) {
        LOG(ERROR) << "binder is nullptr";
    } else {
        LOG(DEBUG) << "linkToDeath";
        mDeathRecipient = AIBinder_DeathRecipient_new(onClientDeath);
        AIBinder_linkToDeath(binder.get(), mDeathRecipient, this);
    }

    LOG(DEBUG) << "FaceAIDLService::setNotifyCallback";
    if (cb == nullptr) {
        LOG(ERROR) << "callback is nullptr";
        return -4;
    }
    mCallback = cb;

    IFaceService* faceAuth = getFaceService();
    if (faceAuth == nullptr) {
        return -4;
    }
    faceAuth->setNotifyCallback(cb);

    LOG(DEBUG) << "FaceAIDLService::setActiveUser";
    LOG(DEBUG) << "(sensorId=" << sensorId << ", userId=" << userId << ")";
    return faceAuth->setActiveUser(userId, sensorId);
}

void FaceAIDLService::cancel(uint32_t cmdId) {
    LOG(DEBUG) << "FaceAIDLService::cancel(cmdId=" << cmdId << ")";
    std::lock_guard<std::mutex> lock(mLock);
    IFaceService* faceAuth = getFaceService();
    if (faceAuth == nullptr) {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    } else {
        faceAuth->cancel(cmdId);
    }
}

::ndk::ScopedAStatus FaceAIDLService::close() {
    LOG(DEBUG) << "FaceAIDLService::close";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        SessionManager::getInstance()->removeClient();
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onSessionClosed();
            }
        } else {
            faceAuth->sessionClose();
            if (mDeathRecipient != nullptr) {
                AIBinder_DeathRecipient_delete(mDeathRecipient);
                mDeathRecipient = nullptr;
            }
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onSessionClosed();
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::generateChallenge() {
    LOG(DEBUG) << "FaceAIDLService::generateChallenge";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            faceAuth->generateChallenge();
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::revokeChallenge(int64_t challenge) {
    LOG(DEBUG) << "FaceAIDLService::revokeChallenge";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            faceAuth->revokeChallenge(static_cast<uint64_t>(challenge));
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::getEnrollmentConfig(
    aidl::android::hardware::biometrics::face::EnrollmentType enrollmentType,
    std::vector<aidl::android::hardware::biometrics::face::EnrollmentStageConfig>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::getEnrollmentConfig";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth == nullptr) {
                if (mCallback != nullptr) {
                    mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
                }
            } else {
                faceAuth->getEnrollmentConfig(enrollmentType, *return_val);
            }
        } else {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::enroll(
    const aidl::android::hardware::keymaster::HardwareAuthToken& hat,
    aidl::android::hardware::biometrics::face::EnrollmentType enrollmentType,
    const std::vector<aidl::android::hardware::biometrics::face::Feature>& toEnroll,
    const std::optional<aidl::android::hardware::common::NativeHandle>& previewSurface,
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::enroll";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                ANativeWindow* window = nullptr;
                if (previewSurface.has_value()) {
                    native_handle_t* nh = ::android::makeFromAidl(previewSurface.value());
                    window = getANativeWindow(nh);
                }
                std::promise<void> promise;
                std::shared_future<void> future = promise.get_future().share();
                hw_auth_token_t hwHat;
                convertHAT(hat, &hwHat);

                uint32_t cmdId = faceAuth->enroll(std::move(promise), hwHat, enrollmentType, toEnroll, window);
                *return_val = ::ndk::SharedRefBase::make<aidl::android::hardware::biometrics::face::CancellationSignal>(
                    future, cmdId, shared_from_this(), mCallback);
                return ::ndk::ScopedAStatus::ok();
            }
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::authenticate(
    int64_t operationId,
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::authenticate";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                std::promise<void> promise;
                std::shared_future<void> future = promise.get_future().share();
                std::vector<uint8_t> emptyVector;

                uint32_t cmdId = faceAuth->authenticate(std::move(promise), static_cast<uint64_t>(operationId), 0, emptyVector);
                *return_val = ::ndk::SharedRefBase::make<aidl::android::hardware::biometrics::face::CancellationSignal>(
                    future, cmdId, shared_from_this(), mCallback);
                return ::ndk::ScopedAStatus::ok();
            }
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::detectInteraction(
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::detectInteraction";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                std::promise<void> promise;
                std::shared_future<void> future = promise.get_future().share();
                uint32_t cmdId = faceAuth->detectInteraction(std::move(promise));
                *return_val = ::ndk::SharedRefBase::make<aidl::android::hardware::biometrics::face::CancellationSignal>(
                    future, cmdId, shared_from_this(), mCallback);
                return ::ndk::ScopedAStatus::ok();
            }
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::enumerateEnrollments() {
    LOG(DEBUG) << "FaceAIDLService::enumerateEnrollments";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            faceAuth->enumerateEnrollments();
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::removeEnrollments(const std::vector<int32_t>& enrollmentIds) {
    LOG(DEBUG) << "FaceAIDLService::removeEnrollments";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            faceAuth->removeEnrollments(enrollmentIds);
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::getFeatures() {
    LOG(DEBUG) << "FaceAIDLService::getFeatures";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            faceAuth->getFeature();
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::setFeature(
    const aidl::android::hardware::keymaster::HardwareAuthToken& hat,
    aidl::android::hardware::biometrics::face::Feature feature,
    bool enabled) {
    LOG(DEBUG) << "FaceAIDLService::setFeature";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            hw_auth_token_t hwHat;
            convertHAT(hat, &hwHat);
            faceAuth->setFeature(hwHat, feature, enabled);
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::getAuthenticatorId() {
    LOG(DEBUG) << "FaceAIDLService::getAuthenticatorId";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            faceAuth->getAuthenticatorId();
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::invalidateAuthenticatorId() {
    LOG(DEBUG) << "FaceAIDLService::invalidateAuthenticatorId";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            faceAuth->invalidateAuthenticatorId();
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::resetLockout(
    const aidl::android::hardware::keymaster::HardwareAuthToken& hat) {
    LOG(DEBUG) << "FaceAIDLService::resetLockout";
    std::lock_guard<std::mutex> lock(mLock);
    if (setupCommunicate() == 0) {
        IFaceService* faceAuth = getFaceService();
        if (faceAuth == nullptr) {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            hw_auth_token_t hwHat;
            convertHAT(hat, &hwHat);
            faceAuth->resetLockout(hwHat);
        }
    } else {
        if (mCallback != nullptr) {
            mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

// Samsung SEH extension methods
::ndk::ScopedAStatus FaceAIDLService::authenticateExtension(
    int64_t operationId, int32_t sensorId,
    const std::vector<uint8_t>& extra,
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::authenticateExtension";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                std::promise<void> promise;
                std::shared_future<void> future = promise.get_future().share();
                uint32_t cmdId = faceAuth->authenticate(
                    std::move(promise), static_cast<uint64_t>(operationId), sensorId, extra);
                *return_val = ::ndk::SharedRefBase::make<aidl::android::hardware::biometrics::face::CancellationSignal>(
                    future, cmdId, shared_from_this(), mCallback);
                return ::ndk::ScopedAStatus::ok();
            }
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::getWrappedData(std::vector<uint8_t>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::getWrappedData";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                faceAuth->getWrappedData(*return_val);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::loadTA(int32_t* return_val) {
    LOG(DEBUG) << "FaceAIDLService::loadTA";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                *return_val = faceAuth->loadTA();
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::unloadTA(int32_t* return_val) {
    LOG(DEBUG) << "FaceAIDLService::unloadTA";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                *return_val = faceAuth->unloadTA();
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::isTAUnloaded(bool* return_val) {
    LOG(DEBUG) << "FaceAIDLService::isTAUnloaded";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                *return_val = faceAuth->isUnLoadTA();
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::pause(int32_t* return_val) {
    LOG(DEBUG) << "FaceAIDLService::pause";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                *return_val = faceAuth->pause();
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::resume(int32_t* return_val) {
    LOG(DEBUG) << "FaceAIDLService::resume";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                *return_val = faceAuth->resume();
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::getFaceTagList(
    std::vector<aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::getFaceTagList";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                faceAuth->getFaceTagList(*return_val);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::getFaceTag(
    aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID tagId,
    aidl::vendor::samsung::hardware::biometrics::face::SehFaceTag* return_val) {
    LOG(DEBUG) << "FaceAIDLService::getFaceTag (tagId=" << static_cast<int>(tagId) << ")";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                return_val->tagId = tagId;
                faceAuth->getFaceTag(tagId, return_val->dataType, return_val->data);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::setFaceTag(
    aidl::vendor::samsung::hardware::biometrics::face::SehFaceTagID tagId,
    const std::vector<uint8_t>& data, int32_t* return_val) {
    LOG(DEBUG) << "FaceAIDLService::setFaceTag (tagId=" << static_cast<int>(tagId) << ")";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                *return_val = faceAuth->setFaceTag(tagId, data);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::setRotation(int32_t rotation, int32_t* return_val) {
    LOG(DEBUG) << "FaceAIDLService::setRotation (rotation=" << rotation << ")";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                *return_val = faceAuth->setOrientation(rotation);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::authenticateForIssuance(
    int64_t operationId, int32_t sensorId,
    const std::vector<uint8_t>& extra, bool isIssuance,
    const std::optional<aidl::android::hardware::common::NativeHandle>& previewSurface,
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::authenticateForIssuance(nativeHandle)";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                ANativeWindow* window = nullptr;
                if (!previewSurface.has_value()) {
                    LOG(DEBUG) << "Current build does not contain NativeHandle";
                } else {
                    native_handle_t* nh = ::android::makeFromAidl(previewSurface.value());
                    window = getANativeWindow(nh);
                }

                std::promise<void> promise;
                std::shared_future<void> future = promise.get_future().share();
                uint32_t cmdId = faceAuth->authenticateForIssuance(
                    std::move(promise), static_cast<uint64_t>(operationId), sensorId, extra, isIssuance, window);
                *return_val = ::ndk::SharedRefBase::make<aidl::android::hardware::biometrics::face::CancellationSignal>(
                    future, cmdId, shared_from_this(), mCallback);
                return ::ndk::ScopedAStatus::ok();
            }
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::getWrappedDataFromMemory(
    aidl::android::hardware::common::Ashmem* return_val) {
    LOG(DEBUG) << "FaceAIDLService::getWrappedDataFromMemory";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                faceAuth->getWrappedDataFromMemory(*return_val);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::getTaInfo(std::string* return_val) {
    LOG(DEBUG) << "FaceAIDLService::getTaInfo";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                *return_val = faceAuth->getTaInfo();
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus FaceAIDLService::authenticateForIssuanceEx(
    int64_t operationId, int32_t sensorId,
    const std::vector<uint8_t>& extra, bool isIssuance,
    const std::optional<aidl::android::hardware::NativeWindow>& previewSurface,
    std::shared_ptr<aidl::android::hardware::biometrics::common::ICancellationSignal>* return_val) {
    LOG(DEBUG) << "FaceAIDLService::authenticateForIssuance(NativeWindow)";
    std::lock_guard<std::mutex> lock(mLock);
    if (isReturnValid(return_val)) {
        if (setupCommunicate() == 0) {
            IFaceService* faceAuth = getFaceService();
            if (faceAuth != nullptr) {
                ANativeWindow* window = nullptr;
                if (previewSurface.has_value()) {
                    window = previewSurface.value().get();
                }

                std::promise<void> promise;
                std::shared_future<void> future = promise.get_future().share();
                uint32_t cmdId = faceAuth->authenticateForIssuance(
                    std::move(promise), static_cast<uint64_t>(operationId), sensorId, extra, isIssuance, window);
                *return_val = ::ndk::SharedRefBase::make<aidl::android::hardware::biometrics::face::CancellationSignal>(
                    future, cmdId, shared_from_this(), mCallback);
                return ::ndk::ScopedAStatus::ok();
            }
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        } else {
            if (mCallback != nullptr) {
                mCallback->onError(aidl::android::hardware::biometrics::face::Error::UNABLE_TO_PROCESS, 0);
            }
        }
    }
    return ::ndk::ScopedAStatus::ok();
}

} // namespace android
