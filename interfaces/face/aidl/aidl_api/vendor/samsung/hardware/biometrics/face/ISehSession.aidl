/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.samsung.hardware.biometrics.face;

import android.hardware.common.Ashmem;
import android.hardware.common.NativeHandle;
import android.hardware.NativeWindow;
import android.hardware.biometrics.common.ICancellationSignal;
import vendor.samsung.hardware.biometrics.face.SehFaceTag;
import vendor.samsung.hardware.biometrics.face.SehFaceTagID;

@VintfStability
interface ISehSession {
    ICancellationSignal authenticateExtension(in long operationId, in int sensorId, in byte[] extra);
    byte[] getWrappedData();
    int loadTA();
    int unloadTA();
    boolean isTAUnloaded();
    int pause();
    int resume();
    SehFaceTagID[] getFaceTagList();
    SehFaceTag getFaceTag(in SehFaceTagID tagId);
    int setFaceTag(in SehFaceTagID tagId, in byte[] data);
    int setRotation(in int rotation);
    void close();
    ICancellationSignal authenticateForIssuance(in long operationId, in int sensorId, in byte[] extra, in boolean isIssuance, in @nullable NativeHandle previewSurface);
    Ashmem getWrappedDataFromMemory();
    String getTaInfo();
    ICancellationSignal authenticateForIssuanceEx(in long operationId, in int sensorId, in byte[] extra, in boolean isIssuance, in @nullable NativeWindow previewSurface);
}
