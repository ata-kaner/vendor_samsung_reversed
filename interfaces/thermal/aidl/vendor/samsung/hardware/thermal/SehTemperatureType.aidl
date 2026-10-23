//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

package vendor.samsung.hardware.thermal;

@VintfStability
@Backing(type="int")
enum SehTemperatureType {
    UNKNOWN = -1,
    CPU = 0,
    GPU = 1,
    BATTERY = 2,
    SKIN = 3,
    USB_PORT = 4,
    POWER_AMPLIFIER = 5,
    BCL_VOLTAGE = 6,
    BCL_CURRENT = 7,
    BCL_PERCENTAGE = 8,
    NPU = 9,
    TPU = 10,
    DISPLAY = 11,
    MODEM = 12,
    SOC = 13,
    SEH_CHARGER = 50,
    SEH_WIFI = 51,
    SEH_FLASH = 52,
    SEH_AMBIENT = 53,
}
