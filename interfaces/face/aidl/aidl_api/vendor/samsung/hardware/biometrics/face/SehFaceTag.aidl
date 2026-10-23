/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.samsung.hardware.biometrics.face;

import vendor.samsung.hardware.biometrics.face.SehFaceTagID;

@VintfStability
parcelable SehFaceTag {
    SehFaceTagID tagId;
    int dataType;
    byte[] data;
}
