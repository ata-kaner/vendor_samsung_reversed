/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef THERMISTORS_H
#define THERMISTORS_H

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "IntervalTimer.h"
#include "Predictor.h"

#if __has_include(<vendor/samsung/hardware/thermal/1.0/types.h>)
#include <vendor/samsung/hardware/thermal/1.0/types.h>
#else
#include "include/vendor/samsung/hardware/thermal/1.0/types.h"
#endif

#if defined(TARGET_USES_EXYNOS_THERMAL) || !defined(TARGET_USES_QCOM_THERMAL)
#if __has_include(<secril-client.h>)
#include <secril-client.h>
#else
#include "include/secril-client.h"
#endif
#endif

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

class Thermistor {
public:
    Thermistor(const std::string& name, SehTemperatureType type);
    virtual ~Thermistor() = default;

    const std::string& getName() const { return mName; }
    SehTemperatureType getType() const { return mType; }

    virtual int get_temperature() = 0;
    virtual bool is_available() const = 0;

    void set_enabled(bool enabled) { mEnabled = enabled; }
    bool is_enabled() const { return mEnabled; }

protected:
    std::string mName;
    SehTemperatureType mType;
    bool mEnabled;
};

class ReadableThermistor : public Thermistor {
public:
    ReadableThermistor(const std::string& name, SehTemperatureType type,
                       const std::vector<std::string>& candidatePaths);
    ReadableThermistor(const std::string& name, SehTemperatureType type,
                       const std::string& exactPath);
    ~ReadableThermistor() override = default;

    int get_temperature() override;
    bool is_available() const override { return mAvailable; }

    const std::string& getPath() const { return mPath; }

private:
    std::string get_available_path(const std::vector<std::string>& candidatePaths);

    std::string mPath;
    bool mAvailable;
};

#if defined(TARGET_USES_EXYNOS_THERMAL)
class VirtualThermistor : public Thermistor {
public:
    VirtualThermistor(const std::string& name, SehTemperatureType type);
    ~VirtualThermistor() override = default;

    void update_temperature(std::map<std::string, int>* temps);
    int get_temperature() override { return mCurrentVal; }
    bool is_available() const override { return mCurrentVal > -300; }

private:
    int mCurrentVal;
};
#endif

class ThermistorGroup {
public:
    ThermistorGroup() = default;
    virtual ~ThermistorGroup() = default;

    virtual void get_temperatures(std::map<std::string, int>* temps);
    virtual void get_types(std::map<std::string, SehTemperatureType>* types);
    virtual void set_enabled(bool enabled);

    void add_thermistor(std::unique_ptr<Thermistor> thermistor);

protected:
    std::vector<std::unique_ptr<Thermistor>> mThermistors;
};

class BasicThermistorGroup : public ThermistorGroup {
public:
    BasicThermistorGroup();
    ~BasicThermistorGroup() override = default;
};

// Qualcomm Modem Thermal Zone implementation (Snapdragon - e.g. r8q)
#if defined(TARGET_USES_QC_THERMAL)
class QcModemThermistorGroup : public ThermistorGroup {
public:
    QcModemThermistorGroup();
    ~QcModemThermistorGroup() override = default;
};
#endif

#if defined(TARGET_USES_EXYNOS_THERMAL)
// Samsung LSI Modem RIL implementation (Exynos - e.g. x1s)
class LsiModemThermistorGroup : public ThermistorGroup {
public:
    LsiModemThermistorGroup();
    ~LsiModemThermistorGroup() override;

    void find_thermistors();
    void get_temperatures(std::map<std::string, int>* temps) override;
    void set_enabled(bool enabled) override;

    static void on_complete(HRilClient client, const void* data, size_t datalen);
    static void retry_find_thermistors(sigval val);

private:
    char* get_raw_cp_temperature();
    void parse_cp_temperature(char* data);
    int request_to_ril();

    HRilClient mRilClient;
    bool mRilConnected;
    std::mutex mRilLock;
    std::condition_variable mRilCv;
    char* mReceivedData;
    bool mRequestPending;
    IntervalTimer mRetryTimer;
    std::map<int, std::string> mSensorIdMap;
    std::map<std::string, int> mSensorTemps;

    static LsiModemThermistorGroup* sInstance;
};
#endif

class VirtualThermistorGroup : public ThermistorGroup {
public:
    VirtualThermistorGroup(const std::string& jsonPath = "/vendor/etc/mercury.json");
    ~VirtualThermistorGroup() override = default;

    void find_thermistors(std::map<std::string, SehTemperatureType>* types);
    void get_temperatures(std::map<std::string, int>* temps) override;
    void get_types(std::map<std::string, SehTemperatureType>* types) override;

private:
    std::unique_ptr<Lrps> mLrps;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor

#endif  // THERMISTORS_H
