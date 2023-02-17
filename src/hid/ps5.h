// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define PS5_OUTPUT_SIZE 78

struct ps5_state {
	bool init;
	bool bluetooth;
};

static void ps5_rumble(struct hid_dev *device, uint16_t low, uint16_t high)
{
	struct ps5_state *ctx = mty_hid_device_get_state(device);

	uint8_t buf[PS5_OUTPUT_SIZE] = {0};
	uint32_t size = 48;
	uint32_t o = 0;

	if (ctx->bluetooth) {
		buf[0] = 0x31;
		size = 78;
		o = 1;
	}

	buf[o + 0] = 0x02;        // Report Type
	buf[o + 1] = 0x01 | 0x02; // Flags0: Both motors
	buf[o + 2] = 0x04 | 0x10; // Flags1: LEDs, player indicator
	buf[o + 3] = high >> 8;   // Right high freq motor
	buf[o + 4] = low >> 8;    // Left low freq motor
	buf[o + 44] = 0x00;       // Player indicator
	buf[o + 45] = 0xFF;       // LED red
	buf[o + 46] = 0x00;       // LED green
	buf[o + 47] = 0xFF;       // LED blue

	if (ctx->bluetooth) {
		uint8_t hdr = 0xA2;
		uint32_t crc = MTY_CRC32(0, &hdr, 1);
		crc = MTY_CRC32(crc, buf, size - 4);
		memcpy(buf + size - 4, &crc, 4);
	}

	mty_hid_device_write(device, buf, size);
}

static void ps5_init(struct hid_dev *device)
{
	struct ps5_state *ctx = mty_hid_device_get_state(device);
	ctx->bluetooth = mty_hid_device_get_input_report_size(device) == 78;

	// Writing a bluetooth output report with id 0x31 will make the DS start sending
	// 0x31 id input reports
	ps5_rumble(device, 0, 0);
}

static bool ps5_state(struct hid_dev *device, const void *data, size_t dsize, MTY_ControllerEvent *c)
{
	bool r = false;
	struct ps5_state *ctx = mty_hid_device_get_state(device);

	if (!ctx->init) {
		// Write once after the first report comes through in case the first
		// write in ps5_init failed
		ps5_init(device);
		ctx->init = true;

	} else {
		const uint8_t *b = data;
		const uint8_t *a = data;
		const uint8_t *t = data;

		// Bluetooth (Simple)
		if (ctx->bluetooth) {

			// Bluetooth (Full)
			if (b[0] == 0x31) {
				b += 4;
				a += 1;
				t -= 2;
				dsize -= 2;
			}

		// Wired (Full)
		} else {
			b += 3;
			t -= 3;
			dsize -= 3;
		}

		c->type = MTY_CTYPE_PS5;
		c->vid = mty_hid_device_get_vid(device);
		c->pid = mty_hid_device_get_pid(device);
		c->numAxes = 7;
		c->numButtons = 15;
		c->id = mty_hid_device_get_id(device);

		c->buttons[MTY_CBUTTON_X] = b[5] & 0x10;
		c->buttons[MTY_CBUTTON_A] = b[5] & 0x20;
		c->buttons[MTY_CBUTTON_B] = b[5] & 0x40;
		c->buttons[MTY_CBUTTON_Y] = b[5] & 0x80;
		c->buttons[MTY_CBUTTON_LEFT_SHOULDER] = b[6] & 0x01;
		c->buttons[MTY_CBUTTON_RIGHT_SHOULDER] = b[6] & 0x02;
		c->buttons[MTY_CBUTTON_LEFT_TRIGGER] = b[6] & 0x04;
		c->buttons[MTY_CBUTTON_RIGHT_TRIGGER] = b[6] & 0x08;
		c->buttons[MTY_CBUTTON_BACK] = b[6] & 0x10;
		c->buttons[MTY_CBUTTON_START] = b[6] & 0x20;
		c->buttons[MTY_CBUTTON_LEFT_THUMB] = b[6] & 0x40;
		c->buttons[MTY_CBUTTON_RIGHT_THUMB] = b[6] & 0x80;
		c->buttons[MTY_CBUTTON_GUIDE] = b[7] & 0x01;
		c->buttons[MTY_CBUTTON_TOUCHPAD] = b[7] & 0x02;

		c->axes[MTY_CAXIS_DPAD].value = b[5] & 0x0F;
		c->axes[MTY_CAXIS_DPAD].usage = 0x39;
		c->axes[MTY_CAXIS_DPAD].min = 0;
		c->axes[MTY_CAXIS_DPAD].max = 7;

		c->axes[MTY_CAXIS_THUMB_LX].value = a[1];
		c->axes[MTY_CAXIS_THUMB_LX].usage = 0x30;
		c->axes[MTY_CAXIS_THUMB_LX].min = 0;
		c->axes[MTY_CAXIS_THUMB_LX].max = UINT8_MAX;
		mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_LX], false);

		c->axes[MTY_CAXIS_THUMB_LY].value = a[2];
		c->axes[MTY_CAXIS_THUMB_LY].usage = 0x31;
		c->axes[MTY_CAXIS_THUMB_LY].min = 0;
		c->axes[MTY_CAXIS_THUMB_LY].max = UINT8_MAX;
		mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_LY], true);

		c->axes[MTY_CAXIS_THUMB_RX].value = a[3];
		c->axes[MTY_CAXIS_THUMB_RX].usage = 0x32;
		c->axes[MTY_CAXIS_THUMB_RX].min = 0;
		c->axes[MTY_CAXIS_THUMB_RX].max = UINT8_MAX;
		mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_RX], false);

		c->axes[MTY_CAXIS_THUMB_RY].value = a[4];
		c->axes[MTY_CAXIS_THUMB_RY].usage = 0x35;
		c->axes[MTY_CAXIS_THUMB_RY].min = 0;
		c->axes[MTY_CAXIS_THUMB_RY].max = UINT8_MAX;
		mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_RY], true);

		c->axes[MTY_CAXIS_TRIGGER_L].value = t[8];
		c->axes[MTY_CAXIS_TRIGGER_L].usage = 0x33;
		c->axes[MTY_CAXIS_TRIGGER_L].min = 0;
		c->axes[MTY_CAXIS_TRIGGER_L].max = UINT8_MAX;

		c->axes[MTY_CAXIS_TRIGGER_R].value = t[9];
		c->axes[MTY_CAXIS_TRIGGER_R].usage = 0x34;
		c->axes[MTY_CAXIS_TRIGGER_R].min = 0;
		c->axes[MTY_CAXIS_TRIGGER_R].max = UINT8_MAX;

		r = true;
	}

	return r;
}
