/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.samsung.hardware.biometrics.face;

import android.hardware.biometrics.face.ISessionCallback;
import vendor.samsung.hardware.biometrics.face.ISehSession;

@VintfStability
interface ISehFace {
    int getServicePid();
    ISehSession createSession(in int sensorId, in int userId, in ISessionCallback cb);
}
