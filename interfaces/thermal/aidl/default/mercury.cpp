//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include "mercury.h"

#include <android/log.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <sstream>

#define LOG_TAG "thermal_mercury"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

static int64_t elapsedRealtime() {
    timespec ts{};
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

// -----------------------------------------------------------------------------
// WeightSetter
// -----------------------------------------------------------------------------

WeightSetter::WeightSetter()
    : mTotalSteps(5), mScenario(0), mWeights{5, 0, 0} {
    mHistory.push_back(0);
}

void WeightSetter::set_scenario(int scenario) {
    if (scenario < 0 || scenario > 2) {
        return;
    }
    mScenario = scenario;
}

void WeightSetter::refresh() {
    auto it = std::find(mHistory.begin(), mHistory.end(), mScenario);
    if (mWeights[mScenario] != mTotalSteps) {
        if (it != mHistory.begin()) {
            if (it != mHistory.end()) {
                mHistory.erase(it);
            }
            mHistory.insert(mHistory.begin(), mScenario);
        }
        mWeights[mScenario]++;
        if (!mHistory.empty()) {
            int lastScenario = mHistory.back();
            mWeights[lastScenario]--;
            if (mWeights[lastScenario] <= 0) {
                mHistory.pop_back();
            }
        }
    }
}

int WeightSetter::get_weight(int scenario) const {
    if (scenario >= 0 && scenario < 3) {
        return mWeights[scenario];
    }
    return 0;
}

void WeightSetter::dump(std::string* out) const {
    char buf[128];
    snprintf(buf, sizeof(buf), "WeightSetter: active scenario=%d, weights=[%d, %d, %d]\n",
             mScenario, mWeights[0], mWeights[1], mWeights[2]);
    out->append(buf);
}

// -----------------------------------------------------------------------------
// Thermistor base
// -----------------------------------------------------------------------------

Thermistor::Thermistor(const std::string& name, SehTemperatureType type)
    : mName(name),
      mType(type),
      mTemperature(-999),
      mAvailable(true),
      mEnabled(true) {}

// -----------------------------------------------------------------------------
// ReadableThermistor
// -----------------------------------------------------------------------------

ReadableThermistor::ReadableThermistor(const std::string& name, SehTemperatureType type,
                                       const std::vector<std::string>& candidatePaths)
    : Thermistor(name, type), mPaths(candidatePaths) {
    for (const auto& path : mPaths) {
        if (access(path.c_str(), R_OK) == 0) {
            mAvailablePath = path;
            mAvailable = true;
            break;
        }
    }
    if (mAvailablePath.empty()) {
        mAvailable = false;
        LOGW("ReadableThermistor: no available path for %s", mName.c_str());
    }
}

int ReadableThermistor::read_sysfs_temperature() {
    if (mAvailablePath.empty()) {
        return -999;
    }
    std::ifstream file(mAvailablePath);
    if (!file.is_open()) {
        return -999;
    }
    int raw = -999;
    if (file >> raw) {
        // Normalize to decicelsius (0.1 C)
        if (raw > 10000 || raw < -10000) {
            return raw / 100;
        } else if (raw > 1000 || raw < -1000) {
            return raw / 10;
        }
        return raw;
    }
    return -999;
}

void ReadableThermistor::update_temperature(std::map<std::string, int>* temps, int /*elapsed*/) {
    int temp = read_sysfs_temperature();
    if (temp != -999) {
        mTemperature = temp;
        (*temps)[mName] = mTemperature;
    }
}

// -----------------------------------------------------------------------------
// ThermalZoneThermistor & ExternalThermistor
// -----------------------------------------------------------------------------

ThermalZoneThermistor::ThermalZoneThermistor(const std::string& name, SehTemperatureType type,
                                             const std::string& zonePath)
    : ReadableThermistor(name, type, {zonePath}) {}

ExternalThermistor::ExternalThermistor(const std::string& name, SehTemperatureType type,
                                       const std::string& sysfsPath)
    : ReadableThermistor(name, type, {sysfsPath}) {}

// -----------------------------------------------------------------------------
// VirtualThermistor
// -----------------------------------------------------------------------------

VirtualThermistor::VirtualThermistor(const std::string& name, SehTemperatureType type)
    : Thermistor(name, type) {}

// -----------------------------------------------------------------------------
// AntennaThermistor
// -----------------------------------------------------------------------------

AntennaThermistor::AntennaThermistor(const std::string& name, const std::string& mainSensor,
                                     const std::string& subSensor, double et)
    : VirtualThermistor(name, SehTemperatureType::UNKNOWN),
      mMain(mainSensor),
      mSub(subSensor),
      mEt(et),
      mFilteredTemp(-999.0) {}

void AntennaThermistor::update_temperature(std::map<std::string, int>* temps, int elapsed) {
    auto itMain = temps->find(mMain);
    if (itMain == temps->end()) {
        return;
    }
    double target = static_cast<double>(itMain->second);
    if (mFilteredTemp == -999.0) {
        mFilteredTemp = target;
    } else {
        int steps = std::max(1, elapsed / 10);
        for (int i = 0; i < steps; ++i) {
            mFilteredTemp += (target - mFilteredTemp) * mEt;
        }
    }
    mTemperature = static_cast<int>(mFilteredTemp);
    (*temps)[mName] = mTemperature;
}

// -----------------------------------------------------------------------------
// CameraThermistor
// -----------------------------------------------------------------------------

CameraThermistor::CameraThermistor(const std::string& name, const std::string& baseSensor,
                                   double offset, double et)
    : VirtualThermistor(name, SehTemperatureType::UNKNOWN),
      mBase(baseSensor),
      mOffset(offset),
      mEt(et),
      mFilteredTemp(-999.0),
      mInitialized(false),
      mCameraListener(std::make_shared<CameraServiceListenerImpl>()) {}

void CameraThermistor::init() {
    if (mInitialized) {
        return;
    }
    mInitialized = true;
}

void CameraThermistor::update_temperature(std::map<std::string, int>* temps, int elapsed) {
    if (!mInitialized) {
        init();
    }
    auto itBase = temps->find(mBase);
    if (itBase == temps->end()) {
        return;
    }
    double baseTemp = static_cast<double>(itBase->second);
    if (mCameraListener && mCameraListener->is_camera_active()) {
        baseTemp += mOffset;
    }

    if (mFilteredTemp == -999.0) {
        mFilteredTemp = baseTemp;
    } else {
        int steps = std::max(1, elapsed / 10);
        for (int i = 0; i < steps; ++i) {
            mFilteredTemp += (baseTemp - mFilteredTemp) * mEt;
        }
    }
    mTemperature = static_cast<int>(mFilteredTemp);
    (*temps)[mName] = mTemperature;
}

// -----------------------------------------------------------------------------
// SpeakerThermistor
// -----------------------------------------------------------------------------

SpeakerThermistor::SpeakerThermistor(const std::string& name, const std::string& basePath,
                                     const std::string& subSensor, double w, double et)
    : VirtualThermistor(name, SehTemperatureType::UNKNOWN),
      mBaseThermistor(std::make_unique<ReadableThermistor>(name + "_base", SehTemperatureType::UNKNOWN,
                                                           std::vector<std::string>{basePath})),
      mSub(subSensor),
      mW(w),
      mEt(et),
      mFilteredTemp(-999.0) {}

void SpeakerThermistor::update_temperature(std::map<std::string, int>* temps, int elapsed) {
    int baseTemp = mBaseThermistor->read_sysfs_temperature();
    auto itSub = temps->find(mSub);
    if (itSub == temps->end()) {
        return;
    }
    double subTemp = static_cast<double>(itSub->second);
    double target = subTemp;
    if (baseTemp != -999 && static_cast<double>(baseTemp) > subTemp) {
        target = subTemp + (static_cast<double>(baseTemp) - subTemp) * mW;
    }

    if (mFilteredTemp == -999.0) {
        mFilteredTemp = target;
    } else {
        int steps = std::max(1, elapsed / 10);
        for (int i = 0; i < steps; ++i) {
            mFilteredTemp += (target - mFilteredTemp) * mEt;
        }
    }
    mTemperature = static_cast<int>(mFilteredTemp);
    (*temps)[mName] = mTemperature;
}

// -----------------------------------------------------------------------------
// AudioThermistor
// -----------------------------------------------------------------------------

AudioThermistor::AudioThermistor(const std::string& name, const std::string& key,
                                 const std::string& refer, double w, double et)
    : VirtualThermistor(name, SehTemperatureType::UNKNOWN),
      mKey(key),
      mRefer(refer),
      mW(w),
      mEt(et),
      mFilteredTemp(-999.0) {}

void AudioThermistor::update_temperature(std::map<std::string, int>* temps, int elapsed) {
    auto itRefer = temps->find(mRefer);
    if (itRefer == temps->end()) {
        return;
    }
    double referTemp = static_cast<double>(itRefer->second);
    if (mFilteredTemp == -999.0) {
        mFilteredTemp = referTemp;
    } else {
        int steps = std::max(1, elapsed / 10);
        for (int i = 0; i < steps; ++i) {
            mFilteredTemp += (referTemp - mFilteredTemp) * mEt;
        }
    }
    mTemperature = static_cast<int>(mFilteredTemp);
    (*temps)[mName] = mTemperature;
}

// -----------------------------------------------------------------------------
// VirtualApThermistor
// -----------------------------------------------------------------------------

VirtualApThermistor::VirtualApThermistor(const std::string& name, const std::string& mainSensor,
                                         double et)
    : VirtualThermistor(name, SehTemperatureType::UNKNOWN),
      mMain(mainSensor),
      mEt(et),
      mFilteredTemp(-999.0) {}

void VirtualApThermistor::update_temperature(std::map<std::string, int>* temps, int elapsed) {
    auto itMain = temps->find(mMain);
    if (itMain == temps->end()) {
        return;
    }
    double target = static_cast<double>(itMain->second);
    if (mFilteredTemp == -999.0) {
        mFilteredTemp = target;
    } else {
        int steps = std::max(1, elapsed / 10);
        for (int i = 0; i < steps; ++i) {
            mFilteredTemp += (target - mFilteredTemp) * mEt;
        }
    }
    mTemperature = static_cast<int>(mFilteredTemp);
    (*temps)[mName] = mTemperature;
}

// -----------------------------------------------------------------------------
// Ambient2Thermistor
// -----------------------------------------------------------------------------

Ambient2Thermistor::Ambient2Thermistor(const std::string& name, const std::string& referSensor,
                                       double weight, double intercept,
                                       const std::map<std::string, double>& variations)
    : VirtualThermistor(name, SehTemperatureType::SEH_AMBIENT),
      mRefer(referSensor),
      mWeight(weight),
      mIntercept(intercept),
      mVariations(variations) {
    for (const auto& pair : mVariations) {
        mVariationBuffers.emplace(pair.first, RingBuffer(60));
    }
}

void Ambient2Thermistor::update_temperature(std::map<std::string, int>* temps, int /*elapsed*/) {
    auto itAp = temps->find("AP");
    auto itRefer = temps->find(mRefer);
    if (itAp == temps->end() || itRefer == temps->end()) {
        return;
    }

    double apTemp = static_cast<double>(itAp->second);
    double referTemp = static_cast<double>(itRefer->second);

    double est = mWeight * referTemp;

    for (const auto& pair : mVariations) {
        const std::string& varName = pair.first;
        double coef = pair.second;
        auto itVar = temps->find(varName);
        if (itVar != temps->end()) {
            double varTemp = static_cast<double>(itVar->second);
            auto& buf = mVariationBuffers[varName];
            buf.add(apTemp - varTemp);
            est += coef * buf.avg();
        }
    }

    est += mIntercept;
    mTemperature = static_cast<int>(est);
    (*temps)[mName] = mTemperature;
}

// -----------------------------------------------------------------------------
// PointPredictor & PointThermistor
// -----------------------------------------------------------------------------

PointPredictor::PointPredictor(const std::vector<HeatmapParameter>& params,
                               const HeatmapParameter& intercept)
    : mParams(params), mIntercept(intercept) {}

int PointPredictor::predict(const std::map<std::string, int>& temps) {
    double sum = 0.0;
    for (const auto& param : mParams) {
        auto it = temps.find(param.name);
        if (it == temps.end() || it->second < -300) {
            return -999;
        }
        double val = static_cast<double>(it->second);
        sum += param.coef * ((val - param.mean) / param.scale);
    }
    double result = (sum + mIntercept.coef) * mIntercept.scale + mIntercept.mean;
    return static_cast<int>(result * 10.0);
}

PointThermistor::PointThermistor(const std::string& name, WeightSetter* ws)
    : VirtualThermistor(name, SehTemperatureType::UNKNOWN), mWeightSetter(ws) {}

void PointThermistor::add_scenario_predictor(int scenario, std::unique_ptr<PointPredictor> pred) {
    mPredictors[scenario] = std::move(pred);
}

void PointThermistor::update_temperature(std::map<std::string, int>* temps, int /*elapsed*/) {
    double weightedSum = 0.0;
    int totalWeight = 0;
    for (const auto& pair : mPredictors) {
        int scenario = pair.first;
        int weight = mWeightSetter ? mWeightSetter->get_weight(scenario) : (scenario == 0 ? 5 : 0);
        if (weight > 0) {
            int predVal = pair.second->predict(*temps);
            if (predVal > -300) {
                weightedSum += predVal * weight;
                totalWeight += weight;
            }
        }
    }
    if (totalWeight > 0) {
        mTemperature = static_cast<int>(weightedSum / totalWeight);
        (*temps)[mName] = mTemperature;
    }
}

// -----------------------------------------------------------------------------
// Aggregation
// -----------------------------------------------------------------------------

Aggregation::Aggregation(const std::string& rep, SehTemperatureType type,
                         const std::vector<std::string>& candidates)
    : VirtualThermistor(rep, type), mCandidates(candidates) {}

void Aggregation::update_temperature(std::map<std::string, int>* temps, int /*elapsed*/) {
    int maxTemp = -999;
    for (const auto& cand : mCandidates) {
        auto it = temps->find(cand);
        if (it != temps->end() && it->second > maxTemp) {
            maxTemp = it->second;
        }
    }
    if (maxTemp != -999) {
        mTemperature = maxTemp;
        (*temps)[mName] = mTemperature;
    }
}

// -----------------------------------------------------------------------------
// Heatsource
// -----------------------------------------------------------------------------

Heatsource::Heatsource(const std::string& name, int gridWidth, int x1, int y1, int x2, int y2)
    : Thermistor(name, SehTemperatureType::UNKNOWN),
      mWidth(gridWidth),
      mX1(x1),
      mY1(y1),
      mX2(x2),
      mY2(y2) {}

void Heatsource::update_temperature(std::map<std::string, int>* temps, int /*elapsed*/) {
    (*temps)[mName] = mTemperature;
}

void Heatsource::update_from_grid(const std::vector<double>& grid) {
    int dx = mX2 - mX1;
    int dy = mY2 - mY1;
    if (dx <= 0 || dy <= 0) {
        return;
    }
    double sum = 0.0;
    for (int y = mY1; y < mY2; ++y) {
        for (int x = mX1; x < mX2; ++x) {
            size_t idx = y * mWidth + x;
            if (idx < grid.size()) {
                sum += grid[idx];
            }
        }
    }
    mTemperature = static_cast<int>(sum / (dx * dy));
}

// -----------------------------------------------------------------------------
// HeatmapPredictor & HeatmapThermistorGroup
// -----------------------------------------------------------------------------

HeatmapPredictor::HeatmapPredictor(int width, int height,
                                   const std::vector<HeatmapParameter>& params,
                                   const std::vector<double>& intercepts)
    : mWidth(width), mHeight(height), mParams(params), mIntercepts(intercepts) {}

std::vector<double> HeatmapPredictor::predict(const std::map<std::string, int>& temps) {
    size_t gridSize = static_cast<size_t>(mWidth * mHeight);
    std::vector<double> grid(gridSize, 0.0);

    for (size_t i = 0; i < gridSize && i < mIntercepts.size(); ++i) {
        grid[i] = mIntercepts[i];
    }

    for (const auto& param : mParams) {
        auto it = temps.find(param.name);
        if (it != temps.end() && it->second > -300) {
            double normalized = (static_cast<double>(it->second) - param.mean) / param.scale;
            for (size_t i = 0; i < gridSize; ++i) {
                grid[i] += param.coef * normalized;
            }
        }
    }
    return grid;
}

HeatmapThermistorGroup::HeatmapThermistorGroup(int width, int height, WeightSetter* ws)
    : mWidth(width),
      mHeight(height),
      mWeightSetter(ws),
      mHeatmapGrid(width * height, 0.0) {}

void HeatmapThermistorGroup::add_heatsource(std::map<std::string, SehTemperatureType>* types,
                                            std::unique_ptr<Heatsource> hs) {
    if (types) {
        (*types)[hs->get_name()] = hs->get_type();
    }
    mHeatsources.push_back(std::move(hs));
}

void HeatmapThermistorGroup::add_scenario_predictor(int scenario,
                                                    std::unique_ptr<HeatmapPredictor> pred) {
    mPredictors[scenario] = std::move(pred);
}

void HeatmapThermistorGroup::update_heatmap(const std::map<std::string, int>& temps) {
    std::fill(mHeatmapGrid.begin(), mHeatmapGrid.end(), 0.0);
    int totalWeight = 0;

    for (const auto& pair : mPredictors) {
        int scenario = pair.first;
        int weight = mWeightSetter ? mWeightSetter->get_weight(scenario) : (scenario == 0 ? 5 : 0);
        if (weight > 0) {
            auto predGrid = pair.second->predict(temps);
            for (size_t i = 0; i < mHeatmapGrid.size() && i < predGrid.size(); ++i) {
                mHeatmapGrid[i] += predGrid[i] * weight;
            }
            totalWeight += weight;
        }
    }

    if (totalWeight > 0) {
        for (double& cell : mHeatmapGrid) {
            cell /= totalWeight;
        }
    }

    for (auto& hs : mHeatsources) {
        hs->update_from_grid(mHeatmapGrid);
    }
}

void HeatmapThermistorGroup::get_temperatures(std::map<std::string, int>& temps) {
    update_heatmap(temps);
    for (auto& hs : mHeatsources) {
        hs->update_temperature(&temps, 0);
    }
    ThermistorGroup::get_temperatures(temps);
}

// -----------------------------------------------------------------------------
// ThermistorGroup
// -----------------------------------------------------------------------------

ThermistorGroup::ThermistorGroup() : mLastUpdateTime(0), mEnabled(true) {}

void ThermistorGroup::add_thermistor(std::map<std::string, SehTemperatureType>* types,
                                     std::unique_ptr<Thermistor> t) {
    if (types) {
        (*types)[t->get_name()] = t->get_type();
    }
    mThermistors.push_back(std::move(t));
}

void ThermistorGroup::get_temperatures(std::map<std::string, int>& temps) {
    if (!mEnabled) {
        return;
    }
    int64_t now = elapsedRealtime();
    int elapsed = mLastUpdateTime == 0 ? 1000 : static_cast<int>(now - mLastUpdateTime);
    mLastUpdateTime = now;

    for (auto& t : mThermistors) {
        if (t->is_enabled()) {
            t->update_temperature(&temps, elapsed);
        }
    }
}

void ThermistorGroup::set_enabled(bool enabled) {
    mEnabled = enabled;
}

// -----------------------------------------------------------------------------
// Parsers & Loaders
// -----------------------------------------------------------------------------

bool readFileToString(const std::string& path, std::string* out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    *out = buffer.str();
    return true;
}

bool parseStringToJson(const std::string& content, Json::Value* out) {
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errs;
    if (!reader->parse(content.data(), content.data() + content.size(), out, &errs)) {
        LOGE("Failed to parse JSON config: %s", errs.c_str());
        return false;
    }
    return true;
}

void createThermalZoneMap(std::map<std::string, std::string>& outMap) {
    outMap.clear();
    for (int i = 0;; ++i) {
        std::string zoneDir = "/sys/class/thermal/thermal_zone" + std::to_string(i);
        std::string typePath = zoneDir + "/type";
        std::string tempPath = zoneDir + "/temp";

        std::ifstream typeFile(typePath);
        if (!typeFile.is_open()) {
            break;
        }
        std::string typeName;
        if (typeFile >> typeName && !typeName.empty()) {
            outMap[typeName] = tempPath;
        }
    }
}

int readPeriod(const Json::Value& root) {
    if (root.isMember("period") && root["period"].isInt()) {
        return root["period"].asInt();
    }
    return 10000;
}

static SehTemperatureType stringToType(const std::string& typeStr) {
    if (typeStr == "CPU" || typeStr == "cpu") return SehTemperatureType::CPU;
    if (typeStr == "GPU" || typeStr == "gpu") return SehTemperatureType::GPU;
    if (typeStr == "BAT" || typeStr == "battery") return SehTemperatureType::BATTERY;
    if (typeStr == "SKIN" || typeStr == "skin") return SehTemperatureType::SKIN;
    if (typeStr == "USB" || typeStr == "usb") return SehTemperatureType::USB_PORT;
    if (typeStr == "PA" || typeStr == "power_amplifier") return SehTemperatureType::POWER_AMPLIFIER;
    if (typeStr == "WIFI" || typeStr == "wifi") return SehTemperatureType::SEH_WIFI;
    if (typeStr == "AMB" || typeStr == "ambient") return SehTemperatureType::SEH_AMBIENT;
    if (typeStr == "CHG" || typeStr == "charger") return SehTemperatureType::SEH_CHARGER;
    return SehTemperatureType::UNKNOWN;
}

std::unique_ptr<ThermistorGroup> parsePhysicalThermistors(
        const Json::Value& root, std::map<std::string, SehTemperatureType>* types) {
    if (!root.isMember("physical") || !root["physical"].isArray()) {
        return nullptr;
    }

    std::map<std::string, std::string> tzMap;
    createThermalZoneMap(tzMap);

    auto group = std::make_unique<ThermistorGroup>();
    for (const auto& item : root["physical"]) {
        std::string name = item["name"].asString();
        std::string path = item["path"].asString();
        std::string typeStr = item.isMember("type") ? item["type"].asString() : "unknown";
        SehTemperatureType type = stringToType(typeStr);

        if (path.find('/') == std::string::npos) {
            // Thermal zone name
            auto it = tzMap.find(path);
            if (it != tzMap.end()) {
                group->add_thermistor(types, std::make_unique<ThermalZoneThermistor>(name, type, it->second));
            } else {
                LOGW("Thermal zone %s not found for %s", path.c_str(), name.c_str());
            }
        } else {
            // Direct sysfs path
            group->add_thermistor(types, std::make_unique<ExternalThermistor>(name, type, path));
        }
    }
    return group;
}

std::unique_ptr<ThermistorGroup> parseVirtualThermistors(
        const Json::Value& root, std::map<std::string, SehTemperatureType>* types) {
    if (!root.isMember("virtual") || !root["virtual"].isArray()) {
        return nullptr;
    }

    auto group = std::make_unique<ThermistorGroup>();
    for (const auto& item : root["virtual"]) {
        std::string name = item["name"].asString();
        std::string type = item["type"].asString();

        if (type == "camera") {
            std::string base = item["base"].asString();
            double offset = item["offset"].asDouble();
            double et = item["Et"].asDouble();
            group->add_thermistor(types, std::make_unique<CameraThermistor>(name, base, offset, et));
        } else if (type == "speaker") {
            std::string base = item["base"].asString();
            std::string sub = item["sub"].asString();
            double w = item["w"].asDouble();
            double et = item["Et"].asDouble();
            group->add_thermistor(types, std::make_unique<SpeakerThermistor>(name, base, sub, w, et));
        } else if (type == "virtual") {
            std::string main = item["main"].asString();
            double et = item["Et"].asDouble();
            group->add_thermistor(types, std::make_unique<VirtualApThermistor>(name, main, et));
        } else if (type == "antenna") {
            std::string main = item["main"].asString();
            std::string sub = item["sub"].asString();
            double et = item["Et"].asDouble();
            group->add_thermistor(types, std::make_unique<AntennaThermistor>(name, main, sub, et));
        } else if (type == "audio") {
            std::string key = item["key"].asString();
            std::string refer = item["refer"].asString();
            double w = item["w"].asDouble();
            double et = item["Et"].asDouble();
            group->add_thermistor(types, std::make_unique<AudioThermistor>(name, key, refer, w, et));
        } else if (type == "ambient2") {
            std::string refer = item["refer"].asString();
            double weight = item["weight"].asDouble();
            double intercept = item["intercept"].asDouble();
            std::map<std::string, double> variations;
            if (item.isMember("variation") && item["variation"].isObject()) {
                for (const auto& varName : item["variation"].getMemberNames()) {
                    variations[varName] = item["variation"][varName].asDouble();
                }
            }
            group->add_thermistor(types, std::make_unique<Ambient2Thermistor>(name, refer, weight, intercept, variations));
        }
    }
    return group;
}

std::unique_ptr<ThermistorGroup> parseLrps(
        const std::map<SehScenarioHint, Json::Value>& scenarios,
        WeightSetter* ws, std::map<std::string, SehTemperatureType>* types) {
    std::map<std::string, PointThermistor*> pointMap;
    auto group = std::make_unique<ThermistorGroup>();

    for (const auto& sPair : scenarios) {
        int scenario = static_cast<int>(sPair.first);
        const auto& root = sPair.second;
        if (!root.isMember("lrp") || !root["lrp"].isArray()) {
            continue;
        }

        for (const auto& item : root["lrp"]) {
            std::string name = item["name"].asString();
            PointThermistor* pt = nullptr;
            auto it = pointMap.find(name);
            if (it == pointMap.end()) {
                auto newPt = std::make_unique<PointThermistor>(name, ws);
                pt = newPt.get();
                pointMap[name] = pt;
                group->add_thermistor(types, std::move(newPt));
            } else {
                pt = it->second;
            }

            std::vector<HeatmapParameter> params;
            if (item.isMember("weights") && item["weights"].isArray()) {
                for (const auto& wItem : item["weights"]) {
                    HeatmapParameter param;
                    param.name = wItem["name"].asString();
                    param.coef = wItem["coef"].asDouble();
                    param.scale = wItem["scale"].asDouble();
                    param.mean = wItem["mean"].asDouble();
                    params.push_back(param);
                }
            }

            HeatmapParameter intercept{};
            if (item.isMember("intercept") && item["intercept"].isObject()) {
                intercept.coef = item["intercept"]["coef"].asDouble();
                intercept.scale = item["intercept"]["scale"].asDouble();
                intercept.mean = item["intercept"]["mean"].asDouble();
            }

            pt->add_scenario_predictor(scenario, std::make_unique<PointPredictor>(params, intercept));
        }
    }
    return group;
}

std::unique_ptr<HeatmapThermistorGroup> parseHeatmap(
        const std::map<SehScenarioHint, Json::Value>& scenarios,
        const std::string& surface, WeightSetter* ws,
        std::map<std::string, SehTemperatureType>* types) {
    auto itNormal = scenarios.find(SehScenarioHint::NORMAL);
    if (itNormal == scenarios.end() || !itNormal->second.isMember("heatmap")) {
        return nullptr;
    }
    const auto& hmRoot = itNormal->second["heatmap"];
    int width = hmRoot["width"].asInt();
    int height = hmRoot["height"].asInt();

    if (!hmRoot.isMember(surface) || !hmRoot[surface].isObject()) {
        return nullptr;
    }

    auto group = std::make_unique<HeatmapThermistorGroup>(width, height, ws);
    const auto& surfObj = hmRoot[surface];

    // Parse heatsources
    if (surfObj.isMember("heatsource") && surfObj["heatsource"].isObject()) {
        char suffix = surface.empty() ? ' ' : std::toupper(surface[0]);
        for (const auto& hsName : surfObj["heatsource"].getMemberNames()) {
            const auto& arr = surfObj["heatsource"][hsName];
            if (arr.isArray() && arr.size() == 4) {
                int x1 = arr[0].asInt();
                int y1 = arr[1].asInt();
                int x2 = arr[2].asInt();
                int y2 = arr[3].asInt();
                std::string fullName = hsName + suffix;
                group->add_heatsource(types, std::make_unique<Heatsource>(fullName, width, x1, y1, x2, y2));
            }
        }
    }

    // Parse scenario predictors
    for (const auto& sPair : scenarios) {
        int scenario = static_cast<int>(sPair.first);
        const auto& sRoot = sPair.second;
        if (!sRoot.isMember("heatmap") || !sRoot["heatmap"].isMember(surface)) {
            continue;
        }
        const auto& sSurf = sRoot["heatmap"][surface];

        std::vector<HeatmapParameter> params;
        if (sSurf.isMember("weights") && sSurf["weights"].isArray()) {
            for (const auto& wItem : sSurf["weights"]) {
                HeatmapParameter param;
                param.name = wItem["name"].asString();
                param.coef = wItem["coef"].asDouble();
                param.scale = wItem["scale"].asDouble();
                param.mean = wItem["mean"].asDouble();
                params.push_back(param);
            }
        }

        std::vector<double> intercepts;
        if (sSurf.isMember("intercept") && sSurf["intercept"].isArray()) {
            for (const auto& val : sSurf["intercept"]) {
                intercepts.push_back(val.asDouble());
            }
        }

        group->add_scenario_predictor(scenario, std::make_unique<HeatmapPredictor>(width, height, params, intercepts));
    }

    return group;
}

std::unique_ptr<ThermistorGroup> parseAggregation(
        const Json::Value& root, std::map<std::string, SehTemperatureType>* types) {
    if (!root.isMember("aggregation") || !root["aggregation"].isArray()) {
        return nullptr;
    }

    auto group = std::make_unique<ThermistorGroup>();
    for (const auto& item : root["aggregation"]) {
        std::string rep = item["rep"].asString();
        std::string typeStr = item.isMember("type") ? item["type"].asString() : "unknown";
        SehTemperatureType type = stringToType(typeStr);

        std::vector<std::string> candidates;
        if (item.isMember("candidates") && item["candidates"].isArray()) {
            for (const auto& cand : item["candidates"]) {
                candidates.push_back(cand.asString());
            }
        }
        group->add_thermistor(types, std::make_unique<Aggregation>(rep, type, candidates));
    }
    return group;
}

void jsonRootMapFromScenarios(std::map<SehScenarioHint, Json::Value>& outMap) {
    outMap.clear();

    const std::map<SehScenarioHint, std::string> scenarioFiles = {
            {SehScenarioHint::NORMAL, "/vendor/etc/mercury.json"},
            {SehScenarioHint::HEAVY, "/vendor/etc/mercury_hv.json"},
    };

    for (const auto& pair : scenarioFiles) {
        std::string content;
        if (readFileToString(pair.second, &content)) {
            Json::Value root;
            if (parseStringToJson(content, &root)) {
                outMap[pair.first] = root;
            } else {
                LOGE("Failed to parse JSON config from %s", pair.second.c_str());
            }
        } else {
            LOGW("Failed to read JSON config from %s", pair.second.c_str());
        }
    }
}

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
