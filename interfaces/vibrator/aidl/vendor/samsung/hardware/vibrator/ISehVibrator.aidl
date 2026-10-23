/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.samsung.hardware.vibrator;

import android.hardware.vibrator.Effect;
import android.hardware.vibrator.IVibratorCallback;
import vendor.samsung.hardware.vibrator.SehAmplitudeType;
import vendor.samsung.hardware.vibrator.SehFifoType;
import vendor.samsung.hardware.vibrator.SehFolderStatus;
import vendor.samsung.hardware.vibrator.SehFolderTentModeStatus;
import vendor.samsung.hardware.vibrator.SehFrequencyType;
import vendor.samsung.hardware.vibrator.SehHapticEnginePacket;
import vendor.samsung.hardware.vibrator.SehHybridHapticEnginePacket;
import vendor.samsung.hardware.vibrator.SehPlaybackPacket;

@VintfStability
interface ISehVibrator {
    boolean supportsAmplitudeControl();
    boolean supportsForceTouchAmplitudeControl();
    boolean supportsFrequencyControl();
    boolean supportsHapticEngine();
    boolean supportsPrebakedHapticPattern();
    boolean supportsFolderStatusSetting();
    boolean supportsEnhancedSamsungHapticPattern();
    void setAmplitude(in int amplitude);
    void setForceTouchAmplitude(in int amplitude);
    void setFrequencyType(in SehFrequencyType type);
    void setFolderStatus(in SehFolderStatus status);
    int getNumberOfPrebakedHapticPattern();
    void performHapticEngine(in SehHapticEnginePacket[] packets, in int repeat);
    void performPrebakedHapticPattern(in int index, in int timeout, in boolean repeat);
    int[] getAmplitudeList(in SehAmplitudeType type);
    int perform(in Effect effect, in int strength, in IVibratorCallback callback);
    void performPlaybackEngine(in SehPlaybackPacket[] packets, in boolean repeat, in int timeout);
    boolean supportsFifoHapticEngine();
    boolean supportsHybridHapticEngine();
    boolean supportsEnhancedHybridHapticPattern();
    void performFifoHapticEngine(in SehFifoType type);
    void performHybridHapticEngine(in SehHybridHapticEnginePacket[] packets, in int repeat);
    boolean supportsFolderTentModeStatusSetting();
    void setFolderTentModeStatus(in SehFolderTentModeStatus status);
}
