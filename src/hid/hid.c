// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "hid.h"
#include "utils.h"

#include <math.h>
#include <string.h>


// Drivers

#include "ps4.h"
#include "ps5.h"
#include "nx.h"
#include "xbox.h"
#include "xboxw.h"

static MTY_CType hid_driver(struct hid_dev *device)
{
	uint16_t vid = mty_hid_device_get_vid(device);
	uint16_t pid = mty_hid_device_get_pid(device);
	uint32_t id = ((uint32_t) vid << 16) | pid;

	switch (id) {
		// Switch
		case 0x057E2009: // Nintendo Switch Pro
		case 0x057E2006: // Nintendo Switch Joycon
		case 0x057E2007: // Nintendo Switch Joycon
		case 0x057E2017: // Nintendo Switch SNES Controller
			return MTY_CTYPE_SWITCH;

		// PS4
		case 0x054C05C4: // Sony DualShock 4 Gen1
		case 0x054C09CC: // Sony DualShock 4 Gen2
		case 0x054C0BA0: // Sony PS4 Controller (Wireless dongle)
			return MTY_CTYPE_PS4;

		// PS5
		case 0x054C0CE6: // Sony DualSense
			return MTY_CTYPE_PS5;

		// Xbox
		case 0x045E02E0: // Microsoft X-Box One S pad (Bluetooth)
		case 0x045E02FD: // Microsoft X-Box One S pad (Bluetooth)
		case 0x045E0B05: // Microsoft X-Box One Elite Series 2 pad (Bluetooth)
		case 0x045E0B13: // Microsoft X-Box Series X (Bluetooth)
			return MTY_CTYPE_XBOX;

		// Xbox Wired
		case 0x045E028E: // Microsoft XBox 360
		case 0x045E028F: // Microsoft XBox 360 v2
		case 0x045E02A1: // Microsoft XBox 360
		case 0x045E0291: // Microsoft XBox 360 Wireless Dongle
		case 0x045E0719: // Microsoft XBox 360 Wireless Dongle
		case 0x045E02A0: // Microsoft Xbox 360 Big Button IR
		case 0x045E02DD: // Microsoft XBox One
		case 0x044FB326: // Microsoft XBox One Firmware 2015
		case 0x045E02E3: // Microsoft XBox One Elite
		case 0x045E02FF: // Microsoft XBox One Elite
		case 0x045E02EA: // Microsoft XBox One S
		case 0x046DC21D: // Logitech F310
		case 0x0E6F02A0: // PDP Xbox One
			return MTY_CTYPE_XBOXW;
	}

	return MTY_CTYPE_DEFAULT;
}

void mty_hid_driver_init(struct hid_dev *device)
{
	switch (hid_driver(device)) {
		case MTY_CTYPE_SWITCH:
			nx_init(device);
			break;
		case MTY_CTYPE_PS4:
			ps4_init(device);
			break;
		case MTY_CTYPE_PS5:
			ps5_init(device);
			break;
		case MTY_CTYPE_XBOX:
			xbox_init(device);
			break;
	}
}

bool mty_hid_driver_state(struct hid_dev *device, const void *buf, size_t size, MTY_ControllerEvent *c)
{
	switch (hid_driver(device)) {
		case MTY_CTYPE_SWITCH:
			return nx_state(device, buf, size, c);
		case MTY_CTYPE_PS4:
			return ps4_state(device, buf, size, c);
		case MTY_CTYPE_PS5:
			return ps5_state(device, buf, size, c);
		case MTY_CTYPE_XBOX:
			return xbox_state(device, buf, size, c);
		case MTY_CTYPE_XBOXW:
			return xboxw_state(device, buf, size, c);
		case MTY_CTYPE_DEFAULT:
			mty_hid_default_state(device, buf, size, c);
			mty_hid_map_axes(c);
			return true;
	}

	return false;
}

void mty_hid_driver_rumble(struct hid *hid, uint32_t id, uint16_t low, uint16_t high)
{
	struct hid_dev *device = mty_hid_get_device_by_id(hid, id);
	if (!device)
		return;

	switch (hid_driver(device)) {
		case MTY_CTYPE_SWITCH:
			nx_rumble(device, low > 0, high > 0);
			break;
		case MTY_CTYPE_PS4:
			ps4_rumble(device, low, high);
			break;
		case MTY_CTYPE_PS5:
			ps5_rumble(device, low, high);
			break;
		case MTY_CTYPE_XBOX:
			xbox_rumble(device, low, high);
			break;
		case MTY_CTYPE_DEFAULT:
			mty_hid_default_rumble(hid, id, low, high);
			break;
	}
}
