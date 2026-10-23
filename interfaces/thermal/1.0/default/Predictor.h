/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <json/json.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

struct FeatureParameter {
    std::string name;
    double coef;
    double scale;
    double mean;
};

struct TargetParameter {
    double scale;
    double mean;
    double intercept;
};

class HeatSource {
public:
    virtual ~HeatSource() = default;
};

class Predictor {
public:
    Predictor(const std::string& name);
    virtual ~Predictor() = default;

    void add_feature(const FeatureParameter& param);
    void set_target(const TargetParameter& target);

    virtual void predict(std::map<std::string, int>* temps);

    const std::string& getName() const { return mName; }

private:
    std::string mName;
    std::vector<FeatureParameter> mFeatures;
    TargetParameter mTarget;
};

class Predictor2D {
public:
    Predictor2D() = default;
    virtual ~Predictor2D() = default;

    void add_heat_source(int index, const std::string& name, HeatSource* source);
    void clear(int index);
    void average_heat_sources(double** matrix, std::map<std::string, int>* temps);
    void predict_heat_sources(std::map<std::string, int>* temps);
    void predict(std::map<std::string, int>* temps);
    void predict_all(std::map<std::string, int>* temps);

private:
    std::map<std::string, std::unique_ptr<HeatSource>> mHeatSources;
};

class Lrps {
public:
    Lrps(const std::string& jsonPath);
    virtual ~Lrps() = default;

    int init_from_json(const std::string& jsonPath);
    void get_temperatures(std::map<std::string, int>* temps);
    void get_types(std::map<std::string, SehTemperatureType>* types);

private:
    std::vector<std::unique_ptr<Predictor>> mPredictors;
    std::unique_ptr<Predictor2D> mPredictor2D;
};

std::unique_ptr<Predictor> parse_lrp(const std::string& name, const Json::Value& json);
std::unique_ptr<Predictor2D> parse_2d_lrp(const Json::Value& json);

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor

#endif  // PREDICTOR_H
