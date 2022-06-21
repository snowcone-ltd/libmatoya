// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static void wsize_max_height(float scale, float max_h, uint32_t screen_h, MTY_Size *size)
{
	if (size->h * scale > max_h * screen_h) {
		float aspect = (float) size->w / size->h;
		size->h = lrint(max_h * screen_h);
		size->w = lrint(size->h * aspect);
	}
}

static void wsize_center(int32_t screen_x, int32_t screen_y, uint32_t screen_w, uint32_t screen_h, MTY_Frame *frame)
{
	if (screen_w > frame->size.w) {
		frame->x += (screen_w - frame->size.w) / 2;

	} else {
		frame->x = screen_x;
		frame->size.w = screen_w;
	}

	if (screen_h > frame->size.h) {
		frame->y += (screen_h - frame->size.h) / 2;

	} else {
		frame->y = screen_y;
		frame->size.h = screen_h;
	}
}
