//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

package vendor.samsung.hardware.thermal;

@VintfStability
parcelable SehTemperature {
    int type;
    String name;
    float value;
}
