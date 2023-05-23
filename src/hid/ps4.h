// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define PS4_OUTPUT_SIZE 78

struct ps4_state {
	bool bluetooth;
};


// Rumble

static void ps4_rumble(struct hid_dev *device, uint16_t low, uint16_t high)
{
	struct ps4_state *ctx = mty_hid_device_get_state(device);

	uint8_t data[PS4_OUTPUT_SIZE] = {0x05, 0x07};
	uint32_t size = 32;
	size_t offset = 4;

	if (ctx->bluetooth) {
		data[0] = 0x11;
		data[1] = 0xC4;
		data[3] = 0x03;

		size = 78;
		offset = 6;
	}

	uint8_t *effects = data + offset;
	effects[0] = high >> 8; // Right
	effects[1] = low >> 8;  // Left
	effects[2] = 0x00;      // LED Red
	effects[3] = 0x00;      // LED Green
	effects[4] = 0x40;      // LED Blue

	if (ctx->bluetooth) {
		uint8_t hdr = 0xA2;
		uint32_t crc = MTY_CRC32(0, &hdr, 1);
		crc = MTY_CRC32(crc, data, size - 4);
		memcpy(data + size - 4, &crc, 4);
	}

	mty_hid_device_write(device, data, size);
}


// State Reports

static void ps4_init(struct hid_dev *device)
{
	struct ps4_state *ctx = mty_hid_device_get_state(device);

	// Bluetooth detection, use the Serial Number feature report
	size_t n = 0;
	uint8_t data[65] = {0x12};
	uint8_t dataz[65] = {0};
	ctx->bluetooth = !mty_hid_device_feature(device, data, 65, &n) && !memcmp(data + 1, dataz + 1, 64);

	// For lights
	ps4_rumble(device, 0, 0);
}

static bool ps4_state(struct hid_dev *device, const void *data, size_t dsize, MTY_ControllerEvent *c)
{
	const uint8_t *d8 = data;

	// Wired
	if (d8[0] == 0x01) {
		d8++;

	// Bluetooth
	} else if (d8[0] == 0x11) {
		d8 += 3;

	} else {
		return false;
	}

	c->type = MTY_CTYPE_PS4;
	c->vid = mty_hid_device_get_vid(device);
	c->pid = mty_hid_device_get_pid(device);
	c->numAxes = 6;
	c->numButtons = 18;
	c->id = mty_hid_device_get_id(device);

	c->buttons[MTY_CBUTTON_X] = d8[4] & 0x10;
	c->buttons[MTY_CBUTTON_A] = d8[4] & 0x20;
	c->buttons[MTY_CBUTTON_B] = d8[4] & 0x40;
	c->buttons[MTY_CBUTTON_Y] = d8[4] & 0x80;
	c->buttons[MTY_CBUTTON_LEFT_SHOULDER] = d8[5] & 0x01;
	c->buttons[MTY_CBUTTON_RIGHT_SHOULDER] = d8[5] & 0x02;
	c->buttons[MTY_CBUTTON_LEFT_TRIGGER] = d8[5] & 0x04;
	c->buttons[MTY_CBUTTON_RIGHT_TRIGGER] = d8[5] & 0x08;
	c->buttons[MTY_CBUTTON_BACK] = d8[5] & 0x10;
	c->buttons[MTY_CBUTTON_START] = d8[5] & 0x20;
	c->buttons[MTY_CBUTTON_LEFT_THUMB] = d8[5] & 0x40;
	c->buttons[MTY_CBUTTON_RIGHT_THUMB] = d8[5] & 0x80;
	c->buttons[MTY_CBUTTON_GUIDE] = d8[6] & 0x01;
	c->buttons[MTY_CBUTTON_TOUCHPAD] = d8[6] & 0x02;

	mty_hid_axis_to_dpad(d8[4] & 0x0F, c);

	c->axes[MTY_CAXIS_THUMB_LX].value = d8[0];
	c->axes[MTY_CAXIS_THUMB_LX].usage = 0x30;
	c->axes[MTY_CAXIS_THUMB_LX].min = 0;
	c->axes[MTY_CAXIS_THUMB_LX].max = UINT8_MAX;
	mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_LX], false);

	c->axes[MTY_CAXIS_THUMB_LY].value = d8[1];
	c->axes[MTY_CAXIS_THUMB_LY].usage = 0x31;
	c->axes[MTY_CAXIS_THUMB_LY].min = 0;
	c->axes[MTY_CAXIS_THUMB_LY].max = UINT8_MAX;
	mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_LY], true);

	c->axes[MTY_CAXIS_THUMB_RX].value = d8[2];
	c->axes[MTY_CAXIS_THUMB_RX].usage = 0x32;
	c->axes[MTY_CAXIS_THUMB_RX].min = 0;
	c->axes[MTY_CAXIS_THUMB_RX].max = UINT8_MAX;
	mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_RX], false);

	c->axes[MTY_CAXIS_THUMB_RY].value = d8[3];
	c->axes[MTY_CAXIS_THUMB_RY].usage = 0x35;
	c->axes[MTY_CAXIS_THUMB_RY].min = 0;
	c->axes[MTY_CAXIS_THUMB_RY].max = UINT8_MAX;
	mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_RY], true);

	c->axes[MTY_CAXIS_TRIGGER_L].value = d8[7];
	c->axes[MTY_CAXIS_TRIGGER_L].usage = 0x33;
	c->axes[MTY_CAXIS_TRIGGER_L].min = 0;
	c->axes[MTY_CAXIS_TRIGGER_L].max = UINT8_MAX;

	c->axes[MTY_CAXIS_TRIGGER_R].value = d8[8];
	c->axes[MTY_CAXIS_TRIGGER_R].usage = 0x34;
	c->axes[MTY_CAXIS_TRIGGER_R].min = 0;
	c->axes[MTY_CAXIS_TRIGGER_R].max = UINT8_MAX;

	return true;
}
