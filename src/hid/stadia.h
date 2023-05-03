// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static bool stadia_state(struct hid_dev *device, const void *data, size_t dsize, MTY_ControllerEvent *c)
{
	if (dsize < 10)
		return false;

	const uint8_t *d8 = data;

	c->type = MTY_CTYPE_STADIA;
	c->vid = mty_hid_device_get_vid(device);
	c->pid = mty_hid_device_get_pid(device);

	c->numAxes = 7;
	c->numButtons = 15;
	c->id = mty_hid_device_get_id(device);

	c->buttons[MTY_CBUTTON_X] = d8[3] & 0x10;
	c->buttons[MTY_CBUTTON_A] = d8[3] & 0x40;
	c->buttons[MTY_CBUTTON_B] = d8[3] & 0x20;
	c->buttons[MTY_CBUTTON_Y] = d8[3] & 0x08;
	c->buttons[MTY_CBUTTON_LEFT_SHOULDER] = d8[3] & 0x04;
	c->buttons[MTY_CBUTTON_RIGHT_SHOULDER] = d8[3] & 0x02;
	c->buttons[MTY_CBUTTON_LEFT_TRIGGER] = d8[2] & 0x04;
	c->buttons[MTY_CBUTTON_RIGHT_TRIGGER] = d8[2] & 0x08;
	c->buttons[MTY_CBUTTON_BACK] = d8[2] & 0x40;
	c->buttons[MTY_CBUTTON_START] = d8[2] & 0x20;
	c->buttons[MTY_CBUTTON_LEFT_THUMB] = d8[3] & 0x01;
	c->buttons[MTY_CBUTTON_RIGHT_THUMB] = d8[2] & 0x80;
	c->buttons[MTY_CBUTTON_GUIDE] = d8[2] & 0x10;
	c->buttons[MTY_CBUTTON_TOUCHPAD] = d8[2] & 0x02;
	c->buttons[MTY_CBUTTON_CAPTURE] = d8[2] & 0x01;

	c->axes[MTY_CAXIS_THUMB_LX].value = d8[4];
	c->axes[MTY_CAXIS_THUMB_LX].usage = 0x30;
	c->axes[MTY_CAXIS_THUMB_LX].min = 1;
	c->axes[MTY_CAXIS_THUMB_LX].max = UINT8_MAX;
	mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_LX], false);

	c->axes[MTY_CAXIS_THUMB_LY].value = d8[5];
	c->axes[MTY_CAXIS_THUMB_LY].usage = 0x31;
	c->axes[MTY_CAXIS_THUMB_LY].min = 1;
	c->axes[MTY_CAXIS_THUMB_LY].max = UINT8_MAX;
	mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_LY], true);

	c->axes[MTY_CAXIS_THUMB_RX].value = d8[6];
	c->axes[MTY_CAXIS_THUMB_RX].usage = 0x32;
	c->axes[MTY_CAXIS_THUMB_RX].min = 1;
	c->axes[MTY_CAXIS_THUMB_RX].max = UINT8_MAX;
	mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_RX], false);

	c->axes[MTY_CAXIS_THUMB_RY].value = d8[7];
	c->axes[MTY_CAXIS_THUMB_RY].usage = 0x35;
	c->axes[MTY_CAXIS_THUMB_RY].min = 1;
	c->axes[MTY_CAXIS_THUMB_RY].max = UINT8_MAX;
	mty_hid_u_to_s16(&c->axes[MTY_CAXIS_THUMB_RY], true);

	c->axes[MTY_CAXIS_TRIGGER_L].value = d8[8];
	c->axes[MTY_CAXIS_TRIGGER_L].usage = 0x33;
	c->axes[MTY_CAXIS_TRIGGER_L].min = 0;
	c->axes[MTY_CAXIS_TRIGGER_L].max = UINT8_MAX;

	c->axes[MTY_CAXIS_TRIGGER_R].value = d8[9];
	c->axes[MTY_CAXIS_TRIGGER_R].usage = 0x34;
	c->axes[MTY_CAXIS_TRIGGER_R].min = 0;
	c->axes[MTY_CAXIS_TRIGGER_R].max = UINT8_MAX;

	c->axes[MTY_CAXIS_DPAD].value = d8[1] & 0x0F;
	c->axes[MTY_CAXIS_DPAD].usage = 0x39;
	c->axes[MTY_CAXIS_DPAD].min = 0;
	c->axes[MTY_CAXIS_DPAD].max = 7;

	return true;
}
