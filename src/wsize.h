// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static void wsize_client(float scale, float max_h, uint32_t screen_h, MTY_Frame *frame)
{
	if (scale == 0.0f)
		scale = 1.0f;

	if (max_h > 0.0f && frame->h * scale > max_h * screen_h) {
		float aspect = (float) frame->w / frame->h;
		frame->h = lrint(max_h * screen_h);
		frame->w = lrint(frame->h * aspect);

	} else {
		frame->w = lrint(frame->w * scale);
		frame->h = lrint(frame->h * scale);
	}
}

static void wsize_center(int32_t screen_x, int32_t screen_y, uint32_t screen_w, uint32_t screen_h, MTY_Frame *frame)
{
	if (screen_w > frame->w) {
		frame->x += (screen_w - frame->w) / 2;

	} else {
		frame->x = screen_x;
		frame->w = screen_w;
	}

	if (screen_h > frame->h) {
		frame->y += (screen_h - frame->h) / 2;

	} else {
		frame->y = screen_y;
		frame->h = screen_h;
	}
}
