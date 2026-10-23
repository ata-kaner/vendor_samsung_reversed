//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <aidl/vendor/samsung/hardware/thermal/SehScenarioHint.h>
#include <aidl/vendor/samsung/hardware/thermal/SehTemperatureType.h>
#include <json/json.h>

#include "CameraServiceListener.h"

namespace vendor {
namespace samsung {
namespace hardware {
namespace thermal {

using ::aidl::vendor::samsung::hardware::thermal::SehScenarioHint;
using ::aidl::vendor::samsung::hardware::thermal::SehTemperatureType;

// Forward declarations
class Thermistor;
class ThermistorGroup;
class WeightSetter;

// Manages scenario hints and smooth weight transitions between scenarios
class WeightSetter {
  public:
    WeightSetter();
    ~WeightSetter() = default;

    void set_scenario(int scenario);
    int get_scenario() const { return mScenario; }
    void refresh();
    int get_weight(int scenario) const;
    void dump(std::string* out) const;

  private:
    int mTotalSteps;
    int mScenario;
    std::vector<int> mHistory;
    int mWeights[3];
};

// Base class for all physical and virtual thermistors
class Thermistor {
  public:
    Thermistor(const std::string& name, SehTemperatureType type);
    virtual ~Thermistor() = default;

    const std::string& get_name() const { return mName; }
    SehTemperatureType get_type() const { return mType; }
    void set_type(SehTemperatureType type) { mType = type; }
    int get_temperature() const { return mTemperature; }
    void set_temperature(int temp) { mTemperature = temp; }
    bool is_available() const { return mAvailable; }
    void set_available(bool avail) { mAvailable = avail; }
    bool is_enabled() const { return mEnabled; }
    void set_enabled(bool enabled) { mEnabled = enabled; }

    virtual void update_temperature(std::map<std::string, int>* temps, int elapsed) = 0;

  protected:
    std::string mName;
    SehTemperatureType mType;
    int mTemperature;
    bool mAvailable;
    bool mEnabled;
};

// Thermistor that reads an integer temperature directly from sysfs
class ReadableThermistor : public Thermistor {
  public:
    ReadableThermistor(const std::string& name, SehTemperatureType type,
                       const std::vector<std::string>& candidatePaths);
    ~ReadableThermistor() override = default;

    int read_sysfs_temperature();
    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;
    const std::string& get_available_path() const { return mAvailablePath; }

  protected:
    std::vector<std::string> mPaths;
    std::string mAvailablePath;
};

// Thermal zone thermistor resolved via /sys/class/thermal/thermal_zone*
class ThermalZoneThermistor : public ReadableThermistor {
  public:
    ThermalZoneThermistor(const std::string& name, SehTemperatureType type,
                          const std::string& zonePath);
};

// External thermistor with explicit sysfs path
class ExternalThermistor : public ReadableThermistor {
  public:
    ExternalThermistor(const std::string& name, SehTemperatureType type,
                       const std::string& sysfsPath);
};

// Base class for all computed virtual thermistors
class VirtualThermistor : public Thermistor {
  public:
    VirtualThermistor(const std::string& name, SehTemperatureType type);
    ~VirtualThermistor() override = default;
};

// Virtual thermistor for antenna
class AntennaThermistor : public VirtualThermistor {
  public:
    AntennaThermistor(const std::string& name, const std::string& mainSensor,
                      const std::string& subSensor, double et);

    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;

  private:
    std::string mMain;
    std::string mSub;
    double mEt;
    double mFilteredTemp;
};

// Virtual thermistor for camera with active camera state offset
class CameraThermistor : public VirtualThermistor {
  public:
    CameraThermistor(const std::string& name, const std::string& baseSensor,
                     double offset, double et);

    void init();
    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;

  private:
    std::string mBase;
    double mOffset;
    double mEt;
    double mFilteredTemp;
    bool mInitialized;
    std::shared_ptr<CameraServiceListenerImpl> mCameraListener;
};

// Virtual thermistor for speaker
class SpeakerThermistor : public VirtualThermistor {
  public:
    SpeakerThermistor(const std::string& name, const std::string& basePath,
                      const std::string& subSensor, double w, double et);

    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;

  private:
    std::unique_ptr<ReadableThermistor> mBaseThermistor;
    std::string mSub;
    double mW;
    double mEt;
    double mFilteredTemp;
};

// Virtual thermistor for audio
class AudioThermistor : public VirtualThermistor {
  public:
    AudioThermistor(const std::string& name, const std::string& key,
                    const std::string& refer, double w, double et);

    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;

  private:
    std::string mKey;
    std::string mRefer;
    double mW;
    double mEt;
    double mFilteredTemp;
};

// Virtual thermistor for AP / PA / WIFI smoothing
class VirtualApThermistor : public VirtualThermistor {
  public:
    VirtualApThermistor(const std::string& name, const std::string& mainSensor, double et);

    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;

  private:
    std::string mMain;
    double mEt;
    double mFilteredTemp;
};

// Moving average ring buffer for ambient variation calculations
struct RingBuffer {
    int capacity;
    int count;
    int index;
    double sum;
    std::vector<double> buffer;

    explicit RingBuffer(int cap = 60)
        : capacity(cap), count(0), index(0), sum(0.0), buffer(cap, 0.0) {}

    void add(double val) {
        if (count < capacity) {
            count++;
        } else {
            sum -= buffer[index];
        }
        buffer[index] = val;
        sum += val;
        index = (index + 1) % capacity;
    }

    double avg() const {
        return count > 0 ? (sum / count) : 0.0;
    }
};

// Ambient 2 model thermistor
class Ambient2Thermistor : public VirtualThermistor {
  public:
    Ambient2Thermistor(const std::string& name, const std::string& referSensor,
                       double weight, double intercept,
                       const std::map<std::string, double>& variations);

    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;

  private:
    std::string mRefer;
    double mWeight;
    double mIntercept;
    std::map<std::string, double> mVariations;
    std::map<std::string, RingBuffer> mVariationBuffers;
};

// Parameter for linear regression
struct HeatmapParameter {
    std::string name;
    double coef;
    double scale;
    double mean;
};

// Predictor for PointThermistor per scenario
class PointPredictor {
  public:
    PointPredictor(const std::vector<HeatmapParameter>& params,
                   const HeatmapParameter& intercept);

    int predict(const std::map<std::string, int>& temps);

  private:
    std::vector<HeatmapParameter> mParams;
    HeatmapParameter mIntercept;
};

// Point thermistor evaluated via regression
class PointThermistor : public VirtualThermistor {
  public:
    PointThermistor(const std::string& name, WeightSetter* ws);

    void add_scenario_predictor(int scenario, std::unique_ptr<PointPredictor> pred);
    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;

  private:
    WeightSetter* mWeightSetter;
    std::map<int, std::unique_ptr<PointPredictor>> mPredictors;
};

// Aggregation thermistor taking the maximum of candidates
class Aggregation : public VirtualThermistor {
  public:
    Aggregation(const std::string& rep, SehTemperatureType type,
                const std::vector<std::string>& candidates);

    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;

  private:
    std::vector<std::string> mCandidates;
};

// Represents a 2D rectangular heat source region on the surface
class Heatsource : public Thermistor {
  public:
    Heatsource(const std::string& name, int gridWidth, int x1, int y1, int x2, int y2);

    void update_temperature(std::map<std::string, int>* temps, int elapsed) override;
    void update_from_grid(const std::vector<double>& grid);

    int x1() const { return mX1; }
    int y1() const { return mY1; }
    int x2() const { return mX2; }
    int y2() const { return mY2; }

  private:
    int mWidth;
    int mX1, mY1, mX2, mY2;
};

// Heatmap surface grid predictor
class HeatmapPredictor {
  public:
    HeatmapPredictor(int width, int height,
                     const std::vector<HeatmapParameter>& params,
                     const std::vector<double>& intercepts);

    std::vector<double> predict(const std::map<std::string, int>& temps);

  private:
    int mWidth;
    int mHeight;
    std::vector<HeatmapParameter> mParams;
    std::vector<double> mIntercepts;
};

// Container managing a group of thermistors
class ThermistorGroup {
  public:
    ThermistorGroup();
    virtual ~ThermistorGroup() = default;

    void add_thermistor(std::map<std::string, SehTemperatureType>* types,
                        std::unique_ptr<Thermistor> t);
    virtual void get_temperatures(std::map<std::string, int>& temps);
    void set_enabled(bool enabled);

  protected:
    std::vector<std::unique_ptr<Thermistor>> mThermistors;
    int64_t mLastUpdateTime;
    bool mEnabled;
};

// Heatmap group predicting 2D surface grid and updating heatsources
class HeatmapThermistorGroup : public ThermistorGroup {
  public:
    HeatmapThermistorGroup(int width, int height, WeightSetter* ws);
    ~HeatmapThermistorGroup() override = default;

    void add_heatsource(std::map<std::string, SehTemperatureType>* types,
                        std::unique_ptr<Heatsource> hs);
    void add_scenario_predictor(int scenario, std::unique_ptr<HeatmapPredictor> pred);
    void get_temperatures(std::map<std::string, int>& temps) override;
    void update_heatmap(const std::map<std::string, int>& temps);

  private:
    int mWidth;
    int mHeight;
    WeightSetter* mWeightSetter;
    std::vector<std::unique_ptr<Heatsource>> mHeatsources;
    std::map<int, std::unique_ptr<HeatmapPredictor>> mPredictors;
    std::vector<double> mHeatmapGrid;
};

// Config loader and parser functions
bool readFileToString(const std::string& path, std::string* out);
bool parseStringToJson(const std::string& content, Json::Value* out);
void createThermalZoneMap(std::map<std::string, std::string>& outMap);
int readPeriod(const Json::Value& root);

std::unique_ptr<ThermistorGroup> parsePhysicalThermistors(
        const Json::Value& root, std::map<std::string, SehTemperatureType>* types);

std::unique_ptr<ThermistorGroup> parseVirtualThermistors(
        const Json::Value& root, std::map<std::string, SehTemperatureType>* types);

std::unique_ptr<ThermistorGroup> parseLrps(
        const std::map<SehScenarioHint, Json::Value>& scenarios,
        WeightSetter* ws, std::map<std::string, SehTemperatureType>* types);

std::unique_ptr<HeatmapThermistorGroup> parseHeatmap(
        const std::map<SehScenarioHint, Json::Value>& scenarios,
        const std::string& surface, WeightSetter* ws,
        std::map<std::string, SehTemperatureType>* types);

std::unique_ptr<ThermistorGroup> parseAggregation(
        const Json::Value& root, std::map<std::string, SehTemperatureType>* types);

void jsonRootMapFromScenarios(std::map<SehScenarioHint, Json::Value>& outMap);

}  // namespace thermal
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
