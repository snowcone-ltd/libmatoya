// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "utils.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>


// Helpers

void mty_hid_u_to_s16(MTY_Axis *a, bool invert)
{
	if (a->max == 0 && a->min == 0)
		return;

	float value = a->value;
	float max = a->max;

	if (a->min < 0) {
		value += (float) abs(a->min);
		max += (float) abs(a->min);

	} else if (a->min > 0) {
		value -= (float) a->min;
		max -= (float) a->min;
	}

	int32_t d = lrint((value / max) * (float) UINT16_MAX);
	a->value = (int16_t) (invert ? -(d - INT16_MAX) : (d - INT16_MAX - 1));
	a->min = INT16_MIN;
	a->max = INT16_MAX;
}

void mty_hid_s_to_s16(MTY_Axis *a)
{
	if (a->value < 0)
		a->value = (int16_t) lrint((float) a->value / (float) a->min * (float) INT16_MIN);

	if (a->value > 0)
		a->value = (int16_t) lrint((float) a->value / (float) a->max * (float) INT16_MAX);

	a->min = INT16_MIN;
	a->max = INT16_MAX;
}

void mty_hid_u_to_u8(MTY_Axis *a)
{
	if (a->max == 0 && a->min == 0)
		return;

	float value = a->value;
	float max = a->max;

	if (a->min < 0) {
		value += (float) abs(a->min);
		max += (float) abs(a->min);

	} else if (a->min > 0) {
		value -= (float) a->min;
		max -= (float) a->min;
	}

	int32_t d = lrint((value / max) * (float) UINT8_MAX);
	a->value = (int16_t) d;
	a->min = 0;
	a->max = UINT8_MAX;
}


// Default mapping

static bool hid_swap_value(MTY_Axis *axes, MTY_CAxis a, MTY_CAxis b)
{
	if (a != b && axes[a].usage != axes[b].usage) {
		MTY_Axis tmp = axes[b];
		axes[b] = axes[a];
		axes[a] = tmp;
		return true;
	}

	return false;
}

static void hid_move_value(MTY_ControllerEvent *c, MTY_Axis *a, uint16_t usage,
	int16_t value, int16_t min, int16_t max)
{
	if (a->usage != usage && c->numAxes < MTY_CAXIS_MAX) {
		if (a->usage != 0x00) {
			MTY_Axis *end = &c->axes[c->numAxes++];
			*end = *a;
		}

		a->usage = usage;
		a->value = value;
		a->min = min;
		a->max = max;

		// Fill in trigger axes if they do not exist
		if (a->usage == 0x33)
			a->value = c->buttons[MTY_CBUTTON_LEFT_TRIGGER] ? UINT8_MAX : 0;

		if (a->usage == 0x34)
			a->value = c->buttons[MTY_CBUTTON_RIGHT_TRIGGER] ? UINT8_MAX : 0;
	}
}

void mty_hid_map_axes(MTY_ControllerEvent *c)
{
	// Make sure there is enough room for the standard CVALUEs
	if (c->numAxes < MTY_CAXIS_DPAD + 1)
		c->numAxes = MTY_CAXIS_DPAD + 1;

	// Swap positions
	for (uint8_t x = 0; x < c->numAxes; x++) {
		retry:

		switch (c->axes[x].usage) {
			case 0x30: // X -> Left Stick X
				if (hid_swap_value(c->axes, x, MTY_CAXIS_THUMB_LX))
					goto retry;
				break;
			case 0x31: // Y -> Left Stick Y
				if (hid_swap_value(c->axes, x, MTY_CAXIS_THUMB_LY))
					goto retry;
				break;
			case 0x32: // Z -> Right Stick X
				if (hid_swap_value(c->axes, x, MTY_CAXIS_THUMB_RX))
					goto retry;
				break;
			case 0x35: // Rz -> Right Stick Y
				if (hid_swap_value(c->axes, x, MTY_CAXIS_THUMB_RY))
					goto retry;
				break;
			case 0x33: // Rx -> Left Trigger
				if (hid_swap_value(c->axes, x, MTY_CAXIS_TRIGGER_L))
					goto retry;
				break;
			case 0x34: // Ry -> Right Trigger
				if (hid_swap_value(c->axes, x, MTY_CAXIS_TRIGGER_R))
					goto retry;
				break;
			case 0x39: // Hat -> DPAD
				if (hid_swap_value(c->axes, x, MTY_CAXIS_DPAD))
					goto retry;
				break;
		}
	}

	// Move axes that are not in the right positions to the end
	for (uint8_t x = 0; x < c->numAxes; x++) {
		MTY_Axis *a = &c->axes[x];

		switch (x) {
			case MTY_CAXIS_THUMB_LX:  hid_move_value(c, a, 0x30, 0, INT16_MIN, INT16_MAX); break;
			case MTY_CAXIS_THUMB_LY:  hid_move_value(c, a, 0x31, 0, INT16_MIN, INT16_MAX); break;
			case MTY_CAXIS_THUMB_RX:  hid_move_value(c, a, 0x32, 0, INT16_MIN, INT16_MAX); break;
			case MTY_CAXIS_THUMB_RY:  hid_move_value(c, a, 0x35, 0, INT16_MIN, INT16_MAX); break;
			case MTY_CAXIS_TRIGGER_L: hid_move_value(c, a, 0x33, 0, 0, UINT8_MAX); break;
			case MTY_CAXIS_TRIGGER_R: hid_move_value(c, a, 0x34, 0, 0, UINT8_MAX); break;
			case MTY_CAXIS_DPAD:      hid_move_value(c, a, 0x39, 8, 0, 7); break;
		}
	}

	// Convert to int16_t
	for (uint8_t x = 0; x < c->numAxes; x++) {
		MTY_Axis *a = &c->axes[x];

		switch (a->usage) {
			case 0x30: // X -> Left Stick X
			case 0x32: // Z -> Right Stick X
				mty_hid_u_to_s16(a, false);
				break;
			case 0x31: // Y -> Left Stick Y
			case 0x35: // Rz -> Right Stick Y
				mty_hid_u_to_s16(a, true);
				break;
			case 0x33: // Rx -> Left Trigger
			case 0x34: // Ry -> Right Trigger
			case 0x36: // Slider
			case 0x37: // Dial
			case 0x38: // Wheel
				mty_hid_u_to_u8(a);
				break;
		}
	}
}


// Deduper

static void hid_clean_value(int16_t *value)
{
	// Dead zone
	if (abs(*value) < 2000)
		*value = 0;

	// Reduce precision
	if (*value != INT16_MIN && *value != INT16_MAX)
		*value &= 0xFFFE;
}

bool mty_hid_dedupe(MTY_Hash *h, MTY_ControllerEvent *c)
{
	MTY_ControllerEvent *prev = MTY_HashGetInt(h, c->id);
	if (!prev) {
		prev = MTY_Alloc(1, sizeof(MTY_ControllerEvent));
		MTY_HashSetInt(h, c->id, prev);
	}

	// Axis dead zone, precision reduction -- helps with deduplication
	hid_clean_value(&c->axes[MTY_CAXIS_THUMB_LX].value);
	hid_clean_value(&c->axes[MTY_CAXIS_THUMB_LY].value);
	hid_clean_value(&c->axes[MTY_CAXIS_THUMB_RX].value);
	hid_clean_value(&c->axes[MTY_CAXIS_THUMB_RY].value);

	// Deduplication
	bool button_diff = memcmp(c->buttons, prev->buttons, c->numButtons * sizeof(bool));
	bool axes_diff = memcmp(c->axes, prev->axes, c->numAxes * sizeof(MTY_Axis));

	*prev = *c;

	return button_diff || axes_diff;
}
