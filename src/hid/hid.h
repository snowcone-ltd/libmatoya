// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

#define HID_STATE_MAX 1024

struct hid;
struct hid_dev;

typedef void (*HID_CONNECT)(struct hid_dev *device, void *opaque);
typedef void (*HID_DISCONNECT)(struct hid_dev *device, void *opaque);
typedef void (*HID_REPORT)(struct hid_dev *device, const void *buf, size_t size, void *opaque);

struct hid *mty_hid_create(HID_CONNECT connect, HID_DISCONNECT disconnect, HID_REPORT report, void *opaque);
struct hid_dev *mty_hid_get_device_by_id(struct hid *ctx, uint32_t id);
void mty_hid_destroy(struct hid **hid);

void mty_hid_device_write(struct hid_dev *ctx, const void *buf, size_t size);
bool mty_hid_device_feature(struct hid_dev *ctx, void *buf, size_t size, size_t *size_out);
void *mty_hid_device_get_state(struct hid_dev *ctx);
uint16_t mty_hid_device_get_vid(struct hid_dev *ctx);
uint16_t mty_hid_device_get_pid(struct hid_dev *ctx);
uint32_t mty_hid_device_get_id(struct hid_dev *ctx);
uint32_t mty_hid_device_get_input_report_size(struct hid_dev *ctx);

void mty_hid_default_state(struct hid_dev *ctx, const void *buf, size_t size, MTY_ControllerEvent *c);
void mty_hid_default_rumble(struct hid *ctx, uint32_t id, uint16_t low, uint16_t high);

void mty_hid_driver_init(struct hid_dev *device);
bool mty_hid_driver_state(struct hid_dev *device, const void *buf, size_t size, MTY_ControllerEvent *c);
void mty_hid_driver_rumble(struct hid *hid, uint32_t id, uint16_t low, uint16_t high);

// Win32 specific for interop with RAWINPUT
void mty_hid_win32_listen(void *hwnd);
void mty_hid_win32_device_change(struct hid *ctx, intptr_t wparam, intptr_t lparam);
void mty_hid_win32_report(struct hid *ctx, intptr_t device, const void *buf, size_t size);
