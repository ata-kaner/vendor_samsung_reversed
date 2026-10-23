/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Thermistors.h"

#include <android/log.h>
#include <sys/stat.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#define LOG_TAG "SehThermal-HAL"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {
namespace V1_0 {
namespace implementation {

// -----------------------------------------------------------------------------
// Thermistor base
// -----------------------------------------------------------------------------

Thermistor::Thermistor(const std::string& name, SehTemperatureType type)
    : mName(name), mType(type), mEnabled(true) {}

// -----------------------------------------------------------------------------
// ReadableThermistor
// -----------------------------------------------------------------------------

ReadableThermistor::ReadableThermistor(const std::string& name, SehTemperatureType type,
                                       const std::vector<std::string>& candidatePaths)
    : Thermistor(name, type), mAvailable(false) {
    mPath = get_available_path(candidatePaths);
}

ReadableThermistor::ReadableThermistor(const std::string& name, SehTemperatureType type,
                                       const std::string& exactPath)
    : Thermistor(name, type), mPath(exactPath), mAvailable(false) {
    std::vector<std::string> single = {exactPath};
    mPath = get_available_path(single);
}

std::string ReadableThermistor::get_available_path(
        const std::vector<std::string>& candidatePaths) {
    for (const auto& path : candidatePaths) {
        struct stat st{};
        if (stat(path.c_str(), &st) != 0) {
            continue;
        }

        FILE* fp = fopen(path.c_str(), "r");
        if (fp != nullptr) {
            fclose(fp);
            mAvailable = true;
            return path;
        }
    }

    mAvailable = false;
    return "";
}

int ReadableThermistor::get_temperature() {
    if (!mAvailable || !mEnabled || mPath.empty()) {
        return -300;
    }

    std::ifstream file(mPath);
    if (!file.is_open()) {
        return -300;
    }

    int rawTemp = -300;
    if (!(file >> rawTemp)) {
        return -300;
    }

    return rawTemp;
}

// -----------------------------------------------------------------------------
// VirtualThermistor
// -----------------------------------------------------------------------------
#if defined(TARGET_USES_EXYNOS_THERMAL)
VirtualThermistor::VirtualThermistor(const std::string& name, SehTemperatureType type)
    : Thermistor(name, type), mCurrentVal(-300) {}

void VirtualThermistor::update_temperature(std::map<std::string, int>* temps) {
    if (temps) {
        auto it = temps->find(mName);
        if (it != temps->end()) {
            mCurrentVal = it->second;
        }
    }
}
#endif

// -----------------------------------------------------------------------------
// ThermistorGroup base
// -----------------------------------------------------------------------------

void ThermistorGroup::add_thermistor(std::unique_ptr<Thermistor> thermistor) {
    if (thermistor) {
        mThermistors.push_back(std::move(thermistor));
    }
}

void ThermistorGroup::get_temperatures(std::map<std::string, int>* temps) {
    if (!temps) return;

    for (const auto& th : mThermistors) {
        if (th->is_enabled() && th->is_available()) {
            (*temps)[th->getName()] = th->get_temperature();
        }
    }
}

void ThermistorGroup::get_types(std::map<std::string, SehTemperatureType>* types) {
    if (!types) return;

    for (const auto& th : mThermistors) {
        if (th->is_available()) {
            (*types)[th->getName()] = th->getType();
        }
    }
}

void ThermistorGroup::set_enabled(bool enabled) {
    for (const auto& th : mThermistors) {
        th->set_enabled(enabled);
    }
}

// -----------------------------------------------------------------------------
// BasicThermistorGroup (Samsung Physical Sysfs Sensors - Shared across SoCs)
// -----------------------------------------------------------------------------

BasicThermistorGroup::BasicThermistorGroup() {
    // AP / CPU
    add_thermistor(std::make_unique<ReadableThermistor>(
            "AP", SehTemperatureType::CPU,
            std::vector<std::string>{
                    "/sys/class/sec/sec-thermistor/temperature",
                    "/sys/devices/platform/sec-thermistor/temperature",
                    "/sys/class/sec/sec-ap-thermistor/temperature"}));

    // BATTERY
    add_thermistor(std::make_unique<ReadableThermistor>(
            "BAT", SehTemperatureType::BATTERY,
            std::vector<std::string>{"/sys/class/power_supply/battery/temp"}));

    // SUBBAT
    add_thermistor(std::make_unique<ReadableThermistor>(
            "SUBBAT", SehTemperatureType::BATTERY,
            std::vector<std::string>{"/sys/class/power_supply/battery/sub_bat_temp"}));

    // SUBBATRAW
    add_thermistor(std::make_unique<ReadableThermistor>(
            "SUBBATRAW", SehTemperatureType::BATTERY,
            std::vector<std::string>{"/sys/class/power_supply/battery/sub_bat_temp_raw"}));

    // USB_PORT
    add_thermistor(std::make_unique<ReadableThermistor>(
            "USB", SehTemperatureType::USB_PORT,
            std::vector<std::string>{"/sys/class/power_supply/battery/usb_temp"}));

    // CHARGER
    add_thermistor(std::make_unique<ReadableThermistor>(
            "CHG", SehTemperatureType::SEH_CHARGER,
            std::vector<std::string>{"/sys/class/power_supply/battery/chg_temp"}));

    // DIRECT CHARGER
    add_thermistor(std::make_unique<ReadableThermistor>(
            "DCHG", SehTemperatureType::SEH_CHARGER,
            std::vector<std::string>{"/sys/class/power_supply/battery/dchg_temp"}));

    // POWER AMPLIFIER
    add_thermistor(std::make_unique<ReadableThermistor>(
            "PA", SehTemperatureType::POWER_AMPLIFIER,
            std::vector<std::string>{
                    "/sys/class/sec/sec-pa-thermistor/temperature",
                    "/sys/class/sec/sec-lte-thermistor/temperature"}));

    // MODEM / CP
    add_thermistor(std::make_unique<ReadableThermistor>(
            "CP", SehTemperatureType::SEH_MODEM,
            std::vector<std::string>{
                    "/sys/class/sec/sec-cp-thermistor/temperature",
                    "/sys/class/sec/sec-nr-thermistor/temperature"}));

    // MILLIMETER WAVE MODEM
    add_thermistor(std::make_unique<ReadableThermistor>(
            "MMW", SehTemperatureType::SEH_MODEM,
            std::vector<std::string>{"/sys/class/sec/sec-mmw-thermistor/temperature"}));

    add_thermistor(std::make_unique<ReadableThermistor>(
            "MMW1", SehTemperatureType::SEH_MODEM,
            std::vector<std::string>{"/sys/class/sec/sec-mmw1-thermistor/temperature"}));

    // WIFI
    add_thermistor(std::make_unique<ReadableThermistor>(
            "WIFI", SehTemperatureType::SEH_WIFI,
            std::vector<std::string>{"/sys/class/sec/sec-wf-thermistor/temperature"}));

    // BACK COVER
    add_thermistor(std::make_unique<ReadableThermistor>(
            "BK", SehTemperatureType::SEH_BACK,
            std::vector<std::string>{
                    "/sys/class/sec/sec-bk-thermistor/temperature",
                    "/sys/class/power_supply/battery/blkt_temp"}));

    // LOW POWER
    add_thermistor(std::make_unique<ReadableThermistor>(
            "LP", SehTemperatureType::SEH_BACK,
            std::vector<std::string>{"/sys/class/sec/sec-lp-thermistor/temperature"}));

    // CAMERA FLASH
    add_thermistor(std::make_unique<ReadableThermistor>(
            "CF", SehTemperatureType::SEH_FLASH,
            std::vector<std::string>{
                    "/sys/class/sec/sec-cf-thermistor/temperature",
                    "/sys/class/sec/sec-flash-thermistor/temperature"}));
}

// -----------------------------------------------------------------------------
// QcModemThermistorGroup (Qualcomm Modem Thermal Zones - Snapdragon)
// -----------------------------------------------------------------------------

#if defined(TARGET_USES_QC_THERMAL)
QcModemThermistorGroup::QcModemThermistorGroup() {
    static const std::map<std::string, std::pair<std::string, SehTemperatureType>> kModemMap = {
            {"modem-0-usr", {"CPTSENS", SehTemperatureType::SEH_MODEM}},
            {"modem-1-usr", {"CPTSENS1", SehTemperatureType::SEH_MODEM}},
            {"modem-ambient-usr", {"ANT0THM", SehTemperatureType::SEH_MODEM}},
            {"modem-mmw0-usr", {"ANT0TSENS", SehTemperatureType::SEH_MODEM}},
            {"modem-mmw1-usr", {"ANT1TSENS", SehTemperatureType::SEH_MODEM}},
            {"modem-mmw2-usr", {"ANT2TSENS", SehTemperatureType::SEH_MODEM}},
            {"modem-lte-sub6-pa2", {"ANT2THM", SehTemperatureType::SEH_MODEM}},
            {"modem-lte-sub6-pa1", {"PATHM", SehTemperatureType::POWER_AMPLIFIER}},
            {"mmw-pa1-usr", {"CPTHM", SehTemperatureType::SEH_MODEM}},
            {"modem-mmw-pa3-usr", {"MMWTHM", SehTemperatureType::SEH_MODEM}},
    };

    char typePath[128];
    char tempPath[128];

    for (int zone = 0; zone < 100; ++zone) {
        snprintf(typePath, sizeof(typePath), "/sys/class/thermal/thermal_zone%d/type", zone);
        snprintf(tempPath, sizeof(tempPath), "/sys/class/thermal/thermal_zone%d/temp", zone);

        std::ifstream typeFile(typePath);
        if (!typeFile.is_open()) {
            break;
        }

        std::string zoneType;
        if (std::getline(typeFile, zoneType)) {
            while (!zoneType.empty() && (zoneType.back() == '\r' || zoneType.back() == '\n' || zoneType.back() == ' ')) {
                zoneType.pop_back();
            }

            auto it = kModemMap.find(zoneType);
            if (it != kModemMap.end()) {
                const std::string& sensorName = it->second.first;
                SehTemperatureType sensorType = it->second.second;

                LOGD("%s, %s -> %s", typePath, zoneType.c_str(), sensorName.c_str());
                add_thermistor(std::make_unique<ReadableThermistor>(
                        sensorName, sensorType, std::string(tempPath)));
            }
        }
    }
}
#endif

// -----------------------------------------------------------------------------
// LsiModemThermistorGroup (Samsung LSI Modem RIL - Exynos)
// -----------------------------------------------------------------------------
#if defined(TARGET_USES_EXYNOS_THERMAL)
LsiModemThermistorGroup* LsiModemThermistorGroup::sInstance = nullptr;

LsiModemThermistorGroup::LsiModemThermistorGroup()
    : mRilClient(nullptr),
      mRilConnected(false),
      mReceivedData(nullptr),
      mRequestPending(false) {
    sInstance = this;

    mRilClient = OpenClient_RILD();
    if (mRilClient != nullptr) {
        find_thermistors();
    } else {
        LOGE("Cannot connect to RILD");
        mRetryTimer.set(5000, &LsiModemThermistorGroup::retry_find_thermistors);
    }
}

LsiModemThermistorGroup::~LsiModemThermistorGroup() {
    mRetryTimer.kill();
    if (mRilClient != nullptr) {
        CloseClient_RILD(mRilClient);
        mRilClient = nullptr;
    }
    sInstance = nullptr;
}

void LsiModemThermistorGroup::retry_find_thermistors(sigval /* val */) {
    if (sInstance) {
        sInstance->find_thermistors();
    }
}

void LsiModemThermistorGroup::find_thermistors() {
    mSensorIdMap = {
            {0x11, "PAMN41"},
            {0x12, "PAMLTE"},
            {0x21, "ANTID1"},
            {0x22, "ANTID2"},
            {0x23, "ANTID3"},
            {0x24, "ANTID4"},
    };

    add_thermistor(std::make_unique<VirtualThermistor>("PAMN41", SehTemperatureType::POWER_AMPLIFIER));
    add_thermistor(std::make_unique<VirtualThermistor>("PAMLTE", SehTemperatureType::POWER_AMPLIFIER));
    add_thermistor(std::make_unique<VirtualThermistor>("ANTID1", SehTemperatureType::SEH_MODEM));
    add_thermistor(std::make_unique<VirtualThermistor>("ANTID2", SehTemperatureType::SEH_MODEM));
    add_thermistor(std::make_unique<VirtualThermistor>("ANTID3", SehTemperatureType::SEH_MODEM));
    add_thermistor(std::make_unique<VirtualThermistor>("ANTID4", SehTemperatureType::SEH_MODEM));
}

void LsiModemThermistorGroup::set_enabled(bool enabled) {
    ThermistorGroup::set_enabled(enabled);
}

void LsiModemThermistorGroup::on_complete(HRilClient /* client */, const void* data, size_t /* datalen */) {
    if (!sInstance) return;

    std::lock_guard<std::mutex> lock(sInstance->mRilLock);
    sInstance->mReceivedData = const_cast<char*>(static_cast<const char*>(data));
    sInstance->mRequestPending = false;
    sInstance->mRilCv.notify_all();
}

int LsiModemThermistorGroup::request_to_ril() {
    if (!mRilClient) return -1;

    if (!isConnected_RILD(mRilClient)) {
        if (Connect_RILD(mRilClient) != 0) {
            LOGE("Cannot connect to RILD");
            return -1;
        }
    }

    // OEM request hook code 0x04000911 for querying Shannon CP temperatures
    uint32_t cmd = 0x04000911;
    return InvokeOemRequestHookRaw(mRilClient, reinterpret_cast<char*>(&cmd), sizeof(cmd));
}

char* LsiModemThermistorGroup::get_raw_cp_temperature() {
    if (!mRilClient) {
        mRilClient = OpenClient_RILD();
        if (!mRilClient) return nullptr;
    }

    std::unique_lock<std::mutex> lock(mRilLock);
    mReceivedData = nullptr;
    mRequestPending = true;

    RegisterRequestCompleteHandler(mRilClient, 59 /* 0x3b */, &LsiModemThermistorGroup::on_complete);

    if (request_to_ril() != 0) {
        mRequestPending = false;
        return nullptr;
    }

    if (mRilCv.wait_for(lock, std::chrono::seconds(1), [this] { return !mRequestPending; })) {
        return mReceivedData;
    }

    LOGE("ril callback has timed out ");
    mRequestPending = false;
    return nullptr;
}

void LsiModemThermistorGroup::parse_cp_temperature(char* data) {
    if (!data) return;

    uint8_t count = static_cast<uint8_t>(data[0]);
    for (uint8_t i = 0; i < count; ++i) {
        int offset = 1 + (i * 4);
        uint8_t sensorId = static_cast<uint8_t>(data[offset]);
        // uint8_t enabled = static_cast<uint8_t>(data[offset + 1]);
        uint8_t tempLow = static_cast<uint8_t>(data[offset + 2]);
        uint8_t tempHigh = static_cast<uint8_t>(data[offset + 3]);

        int16_t rawTemp = static_cast<int16_t>(tempLow | (tempHigh << 8));

        auto it = mSensorIdMap.find(sensorId);
        if (it != mSensorIdMap.end()) {
            mSensorTemps[it->second] = rawTemp;
        }
    }
}

void LsiModemThermistorGroup::get_temperatures(std::map<std::string, int>* temps) {
    if (!temps) return;

    char* data = get_raw_cp_temperature();
    if (data) {
        parse_cp_temperature(data);
    }

    for (const auto& pair : mSensorTemps) {
        (*temps)[pair.first] = pair.second;
    }

    // Update internal virtual thermistors
    for (const auto& th : mThermistors) {
        VirtualThermistor* vt = dynamic_cast<VirtualThermistor*>(th.get());
        if (vt) {
            vt->update_temperature(temps);
        }
    }
}
#endif

// -----------------------------------------------------------------------------
// VirtualThermistorGroup (Samsung Mercury Virtual Predictors)
// -----------------------------------------------------------------------------

VirtualThermistorGroup::VirtualThermistorGroup(const std::string& jsonPath)
    : mLrps(std::make_unique<Lrps>(jsonPath)) {}

void VirtualThermistorGroup::find_thermistors(
        std::map<std::string, SehTemperatureType>* types) {
    get_types(types);
}

void VirtualThermistorGroup::get_temperatures(std::map<std::string, int>* temps) {
    if (mLrps) {
        mLrps->get_temperatures(temps);
    }
    ThermistorGroup::get_temperatures(temps);
}

void VirtualThermistorGroup::get_types(std::map<std::string, SehTemperatureType>* types) {
    if (mLrps) {
        mLrps->get_types(types);
    }
    ThermistorGroup::get_types(types);
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
