// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static MTY_Frame wsize_default(uint32_t screen_w, uint32_t screen_h,
	float scale, float max_h, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
	if (h * scale > max_h * screen_h) {
		float aspect = (float) w / h;
		h = lrint(max_h * screen_h);
		w = lrint(h * aspect);
	}

	if (screen_w > w) {
		x += (screen_w - w) / 2;

	} else {
		x = 0;
		w = screen_w;
	}

	if (screen_h > h) {
		y += (screen_h - h) / 2;

	} else {
		y = 0;
		h = screen_h;
	}

	return (MTY_Frame) {
		.x = x,
		.y = y,
		.size.w = w,
		.size.h = h,
	};
}
