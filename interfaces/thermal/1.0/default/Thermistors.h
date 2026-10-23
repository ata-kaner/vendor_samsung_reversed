/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef THERMISTORS_H
#define THERMISTORS_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Predictor.h"
#if __has_include(<vendor/samsung/hardware/thermal/1.0/types.h>)
#include <vendor/samsung/hardware/thermal/1.0/types.h>
#else
#include "include/vendor/samsung/hardware/thermal/1.0/types.h"
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

class QcModemThermistorGroup : public ThermistorGroup {
public:
    QcModemThermistorGroup();
    ~QcModemThermistorGroup() override = default;
};

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
