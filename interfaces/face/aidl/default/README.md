# Samsung Biometrics Face AIDL Service

Reconstructed implementation of Samsung's proprietary AIDL Face HAL daemon (`vendor.samsung.hardware.biometrics.face-service`).

## Architecture Overview

```
                          [ Android Framework ]
                                   │
              ┌────────────────────┴────────────────────┐
              ▼                                         ▼
   android.hardware.biometrics.face            vendor.samsung.hardware.biometrics.face
           (IFace / ISession)                         (ISehFace / ISehSession)
              │                                         │
              │                                         │ (AIBinder_setExtension)
              └────────────────────┬────────────────────┘
                                   ▼
                            FaceAIDLService
                                   │
                                   │ (dlopen / dlsym)
                                   ▼
                           libFaceService.so
                       (android::FaceService)
                                   │
                        ┌──────────┴──────────┐
                        ▼                     ▼
               libsecfr_engine.so      libsecfr_model.so
```

### Components:
- **`main.cpp`**: Configures binder threadpool, registers `IFace/default`, attaches `ISehFace` as extension via `AIBinder_setExtension`.
- **`Face` / `SehFace`**: HAL service entrypoints managing sensor properties and session creation.
- **`Session` / `SehSession`**: Session instances implementing standard AOSP and Samsung extension methods.
- **`SessionManager`**: Singleton maintaining client references and binder linkages.
- **`FaceAIDLService`**: Dynamic loader and bridge to `libFaceService.so` via `IFaceService`.
- **`CancellationSignal`**: Asynchronous command cancellation handling.
