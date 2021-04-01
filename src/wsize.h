// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static void wsize_client(const MTY_WindowDesc *desc, float scale, int32_t screen_h,
	int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
	if (desc->maxHeight > 0.0f && (float) desc->height * scale >
		desc->maxHeight * (float) screen_h)
	{
		float aspect = (float) desc->width / (float) desc->height;
		*h = lrint(desc->maxHeight * (float) screen_h);
		*w = lrint((float) *h * aspect);

	} else {
		*w = lrint((float) desc->width * scale);
		*h = lrint((float) desc->height * scale);
	}
}

static void wsize_center(int32_t screen_x, int32_t screen_y, int32_t screen_w, int32_t screen_h,
	int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
	if (screen_w > *w) {
		*x += (screen_w - *w) / 2;

	} else {
		*x = screen_x;
		*w = screen_w;
	}

	if (screen_h > *h) {
		*y += (screen_h - *h) / 2;

	} else {
		*y = screen_y;
		*h = screen_h;
	}
}
