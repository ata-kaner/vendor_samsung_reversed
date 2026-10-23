/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Thermistors.h"

#include <android/log.h>
#include <sys/stat.h>

#include <cstdio>
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
// BasicThermistorGroup (Samsung Physical Sysfs Sensors)
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
// QcModemThermistorGroup (Qualcomm Modem Thermal Zones)
// -----------------------------------------------------------------------------

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
            // Trim trailing newline or whitespace
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
