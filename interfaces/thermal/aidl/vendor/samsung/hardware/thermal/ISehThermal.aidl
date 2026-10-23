//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

package vendor.samsung.hardware.thermal;

import vendor.samsung.hardware.thermal.ISehThermalChangedCallback;
import vendor.samsung.hardware.thermal.SehTemperature;

@VintfStability
interface ISehThermal {
    SehTemperature[] getTemperatures();
    SehTemperature[] getTemperaturesWithType(in int type);
    void registerThermalChangedCallback(in ISehThermalChangedCallback callback);
    void unregisterThermalChangedCallback(in ISehThermalChangedCallback callback);
    oneway void setScenarioHint(in int hint);
    int getScenarioHint();
}
