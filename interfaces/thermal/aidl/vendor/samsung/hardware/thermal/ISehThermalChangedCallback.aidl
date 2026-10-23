//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

package vendor.samsung.hardware.thermal;

import vendor.samsung.hardware.thermal.SehTemperature;

@VintfStability
interface ISehThermalChangedCallback {
    oneway void notifyTemperatures(in SehTemperature[] temperatures);
}
