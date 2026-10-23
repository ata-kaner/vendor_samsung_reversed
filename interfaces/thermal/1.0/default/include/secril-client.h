/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SECRIL_CLIENT_H
#define SECRIL_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* HRilClient;

HRilClient OpenClient_RILD(void);
int Connect_RILD(HRilClient client);
int isConnected_RILD(HRilClient client);
int Disconnect_RILD(HRilClient client);
int CloseClient_RILD(HRilClient client);

typedef void (*RilCompleteHandler)(HRilClient client, const void* data, size_t datalen);
typedef void (*RilErrorHandler)(void* cookie);
typedef void (*RilUnsolicitedHandler)(HRilClient client, const void* data, size_t datalen);

int RegisterRequestCompleteHandler(HRilClient client, uint32_t id, RilCompleteHandler handler);
int RegisterErrorCallback(HRilClient client, RilErrorHandler handler);
int RegisterUnsolicitedHandler(HRilClient client, uint32_t id, RilUnsolicitedHandler handler);

int InvokeOemRequestHookRaw(HRilClient client, const char* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // SECRIL_CLIENT_H
