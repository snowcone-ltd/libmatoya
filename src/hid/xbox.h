// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

enum xbox_proto {
	XBOX_PROTO_UNKNOWN = 0,
	XBOX_PROTO_V1,
	XBOX_PROTO_V2,
};

struct xbox_state {
	enum xbox_proto proto;
	bool series_x;
	bool guide;
	bool rumble;
	MTY_Time rumble_ts;
	uint16_t low;
	uint16_t high;
};


// Rumble

static void xbox_rumble(struct hid_dev *device, uint16_t low, uint16_t high)
{
	struct xbox_state *ctx = mty_hid_device_get_state(device);

	if (low != ctx->low || high != ctx->high) {
		ctx->low = low;
		ctx->high = high;
		ctx->rumble = true;
	}
}

static void xbox_do_rumble(struct hid_dev *device)
{
	struct xbox_state *ctx = mty_hid_device_get_state(device);

	MTY_Time ts = MTY_GetTime();
	double diff = MTY_TimeDiff(ctx->rumble_ts, ts);
	bool non_zero = ctx->low > 0 || ctx->high > 0;

	// Xbox one seems to have an internal timeout of 3s
	if ((ctx->rumble && diff > 50.0f) || (non_zero && diff > 2000.0f)) {
		uint8_t rumble[9] = {0x03, 0x0F,
			0x00, // Left trigger motor
			0x00, // Right trigger motor
			0x00, // Left motor
			0x00, // Right motor
			0xFF,
			0x00,
			0x00,
		};

		rumble[4] = ctx->low >> 8;
		rumble[5] = ctx->high >> 8;

		mty_hid_device_write(device, rumble, 9);
		ctx->rumble_ts = ts;
		ctx->rumble = false;
	}
}


// State Reports

static void xbox_init(struct hid_dev *device)
{
	struct xbox_state *ctx = mty_hid_device_get_state(device);
	ctx->series_x = mty_hid_device_get_pid(device) == 0x0B13;
	ctx->rumble = true;
}

static bool xbox_state(struct hid_dev *device, const void *data, size_t dsize, MTY_ControllerEvent *c)
{
	bool r = false;
	struct xbox_state *ctx = mty_hid_device_get_state(device);
	const uint8_t *d8 = data;

	if (d8[0] == 0x01) {
		c->type = MTY_CTYPE_XBOX;
		c->vid = mty_hid_device_get_vid(device);
		c->pid = mty_hid_device_get_pid(device);
		c->numAxes = 7;
		c->numButtons = 14;
		c->id = mty_hid_device_get_id(device);

		if (ctx->proto == XBOX_PROTO_UNKNOWN && d8[15] & 0x10)
			ctx->proto = XBOX_PROTO_V2;

		if (ctx->proto == XBOX_PROTO_V2)
			ctx->guide = d8[15] & 0x10;

		c->buttons[MTY_CBUTTON_X] = d8[14] & 0x08;
		c->buttons[MTY_CBUTTON_A] = d8[14] & 0x01;
		c->buttons[MTY_CBUTTON_B] = d8[14] & 0x02;
		c->buttons[MTY_CBUTTON_Y] = d8[14] & 0x10;
		c->buttons[MTY_CBUTTON_LEFT_SHOULDER] = d8[14] & 0x40;
		c->buttons[MTY_CBUTTON_RIGHT_SHOULDER] = d8[14] & 0x80;
		c->buttons[MTY_CBUTTON_BACK] = ctx->series_x ? d8[15] & 0x04 : d8[16] & 0x01;
		c->buttons[MTY_CBUTTON_START] = d8[15] & 0x08;
		c->buttons[MTY_CBUTTON_LEFT_THUMB] = d8[15] & 0x20;
		c->buttons[MTY_CBUTTON_RIGHT_THUMB] = d8[15] & 0x40;
		c->buttons[MTY_CBUTTON_GUIDE] = ctx->guide;
		c->buttons[MTY_CBUTTON_TOUCHPAD] = ctx->series_x ? d8[16] & 0x01 : 0;

		c->axes[MTY_CAXIS_THUMB_LX].value = *((int16_t *) (d8 + 1)) - 0x8000;
		c->axes[MTY_CAXIS_THUMB_LX].usage = 0x30;
		c->axes[MTY_CAXIS_THUMB_LX].min = INT16_MIN;
		c->axes[MTY_CAXIS_THUMB_LX].max = INT16_MAX;

		int16_t ly = *((int16_t *) (d8 + 3)) - 0x8000;
		c->axes[MTY_CAXIS_THUMB_LY].value = (int16_t) ((int32_t) (ly + 1) * -1);
		c->axes[MTY_CAXIS_THUMB_LY].usage = 0x31;
		c->axes[MTY_CAXIS_THUMB_LY].min = INT16_MIN;
		c->axes[MTY_CAXIS_THUMB_LY].max = INT16_MAX;

		c->axes[MTY_CAXIS_THUMB_RX].value = *((int16_t *) (d8 + 5)) - 0x8000;
		c->axes[MTY_CAXIS_THUMB_RX].usage = 0x32;
		c->axes[MTY_CAXIS_THUMB_RX].min = INT16_MIN;
		c->axes[MTY_CAXIS_THUMB_RX].max = INT16_MAX;

		int16_t ry = *((int16_t *) (d8 + 7)) - 0x8000;
		c->axes[MTY_CAXIS_THUMB_RY].value = (int16_t) ((int32_t) (ry + 1) * -1);
		c->axes[MTY_CAXIS_THUMB_RY].usage = 0x35;
		c->axes[MTY_CAXIS_THUMB_RY].min = INT16_MIN;
		c->axes[MTY_CAXIS_THUMB_RY].max = INT16_MAX;

		c->axes[MTY_CAXIS_TRIGGER_L].value = *((uint16_t *) (d8 + 9)) >> 2;;
		c->axes[MTY_CAXIS_TRIGGER_L].usage = 0x33;
		c->axes[MTY_CAXIS_TRIGGER_L].min = 0;
		c->axes[MTY_CAXIS_TRIGGER_L].max = UINT8_MAX;

		c->axes[MTY_CAXIS_TRIGGER_R].value = *((uint16_t *) (d8 + 11)) >> 2;;
		c->axes[MTY_CAXIS_TRIGGER_R].usage = 0x34;
		c->axes[MTY_CAXIS_TRIGGER_R].min = 0;
		c->axes[MTY_CAXIS_TRIGGER_R].max = UINT8_MAX;

		c->buttons[MTY_CBUTTON_LEFT_TRIGGER] = c->axes[MTY_CAXIS_TRIGGER_L].value > 0;
		c->buttons[MTY_CBUTTON_RIGHT_TRIGGER] = c->axes[MTY_CAXIS_TRIGGER_R].value > 0;

		c->axes[MTY_CAXIS_DPAD].value = d8[13] > 0 ? d8[13] - 1 : 8;
		c->axes[MTY_CAXIS_DPAD].usage = 0x39;
		c->axes[MTY_CAXIS_DPAD].min = 0;
		c->axes[MTY_CAXIS_DPAD].max = 7;

		r = true;

	} else if (d8[0] == 0x02) {
		ctx->proto = XBOX_PROTO_V2;
		ctx->guide = d8[1] & 0x01;
	}

	xbox_do_rumble(device);

	return r;
}
