/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Predictor.h"

#include <android/log.h>
#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <fstream>

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
// Predictor (1D Linear Regression)
// -----------------------------------------------------------------------------

Predictor::Predictor(const std::string& name)
    : mName(name), mTarget{1.0, 0.0, 0.0} {}

void Predictor::add_feature(const FeatureParameter& param) {
    mFeatures.push_back(param);
}

void Predictor::set_target(const TargetParameter& target) {
    mTarget = target;
}

void Predictor::predict(std::map<std::string, int>* temps) {
    if (!temps) return;

    double normalizedSum = 0.0;

    for (const auto& feat : mFeatures) {
        auto it = temps->find(feat.name);
        if (it == temps->end() || it->second <= -301) {
            // Missing or invalid sensor reading (-30.0 C / -300 deci-Celsius or below)
            return;
        }

        double rawDeciC = static_cast<double>(it->second);
        double normalizedInput = (rawDeciC - feat.mean) / feat.scale;
        normalizedSum += feat.coef * normalizedInput;
    }

    // Denormalize: (y + intercept) * scale_Y + mean_Y
    double predictedCelsius = (normalizedSum + mTarget.intercept) * mTarget.scale + mTarget.mean;

    // Convert Celsius to deci-Celsius (0.1 deg C)
    int predictedDeciC = static_cast<int>(std::round(predictedCelsius * 10.0));
    (*temps)[mName] = predictedDeciC;
}

// -----------------------------------------------------------------------------
// Predictor2D (Heatmap Framework)
// -----------------------------------------------------------------------------

void Predictor2D::add_heat_source(int /* index */, const std::string& name, HeatSource* source) {
    if (source) {
        mHeatSources[name] = std::unique_ptr<HeatSource>(source);
    }
}

void Predictor2D::clear(int /* index */) {
    mHeatSources.clear();
}

void Predictor2D::average_heat_sources(double** /* matrix */, std::map<std::string, int>* /* temps */) {
    // 2D heatmap matrix averaging
}

void Predictor2D::predict_heat_sources(std::map<std::string, int>* /* temps */) {
    // 2D heat source projection
}

void Predictor2D::predict(std::map<std::string, int>* /* temps */) {
    // 2D heatmap interpolation
}

void Predictor2D::predict_all(std::map<std::string, int>* temps) {
    predict_heat_sources(temps);
    predict(temps);
}

// -----------------------------------------------------------------------------
// JSON Parsers
// -----------------------------------------------------------------------------

std::unique_ptr<Predictor> parse_lrp(const std::string& name, const Json::Value& json) {
    if (!json.isArray()) {
        return nullptr;
    }

    auto predictor = std::make_unique<Predictor>(name);

    for (const auto& item : json) {
        if (!item.isObject() || !item.isMember("name")) {
            continue;
        }

        std::string featName = item["name"].asString();
        if (featName == "Y") {
            TargetParameter target{};
            target.scale = item.get("scale", 1.0).asDouble();
            target.mean = item.get("mean", 0.0).asDouble();
            target.intercept = item.get("intercept", 0.0).asDouble();
            predictor->set_target(target);
        } else {
            FeatureParameter param{};
            param.name = featName;
            param.coef = item.get("coef", 0.0).asDouble();
            param.scale = item.get("scale", 1.0).asDouble();
            param.mean = item.get("mean", 0.0).asDouble();

            LOGD("%5s : %1.7lf | %3.7lf | %3.7lf",
                 param.name.c_str(), param.coef, param.scale, param.mean);
            predictor->add_feature(param);
        }
    }

    return predictor;
}

std::unique_ptr<Predictor2D> parse_2d_lrp(const Json::Value& /* json */) {
    return std::make_unique<Predictor2D>();
}

// -----------------------------------------------------------------------------
// Lrps Manager
// -----------------------------------------------------------------------------

Lrps::Lrps(const std::string& jsonPath) {
    int ret = init_from_json(jsonPath);
    LOGD("RESULT: init_from_json, %d", ret);
}

int Lrps::init_from_json(const std::string& jsonPath) {
    struct stat st{};
    if (stat(jsonPath.c_str(), &st) != 0) {
        LOGE("mercury json not found: %s", jsonPath.c_str());
        return 1;
    }

    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        LOGE("failed to open mercury json: %s", jsonPath.c_str());
        return 2;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &root, &errs) || !root.isObject()) {
        LOGE("json parsing fail: %s", errs.c_str());
        return 3;
    }

    for (const auto& modelName : root.getMemberNames()) {
        const auto& modelJson = root[modelName];
        if (modelName == "heatmap" || modelName == "virtual_2d") {
            mPredictor2D = parse_2d_lrp(modelJson);
        } else if (modelJson.isArray()) {
            auto predictor = parse_lrp(modelName, modelJson);
            if (predictor) {
                mPredictors.push_back(std::move(predictor));
            }
        }
    }

    return 0;
}

void Lrps::get_temperatures(std::map<std::string, int>* temps) {
    if (!temps) return;

    for (const auto& predictor : mPredictors) {
        predictor->predict(temps);
    }

    if (mPredictor2D) {
        mPredictor2D->predict_all(temps);
    }

    // Samsung Mercury Surface Skin Aggregation:
    // Front skin = max(BLRF, LRF)
    // Back skin  = max(BLRB, LRB)
    // Overall skin = max(SKINF, SKINB)
    int frontSkin = -300;
    int backSkin = -300;

    auto itBLRF = temps->find("BLRF");
    auto itLRF  = temps->find("LRF");
    if (itBLRF != temps->end() && itBLRF->second > -300) {
        frontSkin = std::max(frontSkin, itBLRF->second);
    }
    if (itLRF != temps->end() && itLRF->second > -300) {
        frontSkin = std::max(frontSkin, itLRF->second);
    }
    if (frontSkin > -300) {
        (*temps)["SKINF"] = frontSkin;
    }

    auto itBLRB = temps->find("BLRB");
    auto itLRB  = temps->find("LRB");
    if (itBLRB != temps->end() && itBLRB->second > -300) {
        backSkin = std::max(backSkin, itBLRB->second);
    }
    if (itLRB != temps->end() && itLRB->second > -300) {
        backSkin = std::max(backSkin, itLRB->second);
    }
    if (backSkin > -300) {
        (*temps)["SKINB"] = backSkin;
    }

    int overallSkin = std::max(frontSkin, backSkin);
    if (overallSkin > -300) {
        (*temps)["SKIN"] = overallSkin;
        (*temps)["LRP"] = overallSkin;
        (*temps)["LRP2"] = overallSkin;
    }
}

void Lrps::get_types(std::map<std::string, SehTemperatureType>* types) {
    if (!types) return;

    for (const auto& predictor : mPredictors) {
        const std::string& name = predictor->getName();
        if (name.find("AP") != std::string::npos) {
            (*types)[name] = SehTemperatureType::CPU;
        } else {
            (*types)[name] = SehTemperatureType::SKIN;
        }
    }

    (*types)["SKINF"] = SehTemperatureType::SKIN;
    (*types)["SKINB"] = SehTemperatureType::SEH_BACK;
    (*types)["SKIN"]  = SehTemperatureType::SKIN;
    (*types)["LRP"]   = SehTemperatureType::SKIN;
    (*types)["LRP2"]  = SehTemperatureType::SKIN;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
