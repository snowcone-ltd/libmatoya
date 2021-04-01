// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <math.h>
#include <string.h>

static void image_center_crop(uint32_t w, uint32_t h, uint32_t target_w, uint32_t target_h,
	uint32_t *crop_w, uint32_t *crop_h)
{
	float m = 1.0f;

	if (w < target_w) {
		m = (float) target_w / (float) w;
		w = target_w;
		h = lrint((float) h * m);
	}

	if (h < target_h) {
		m = (float) target_h / (float) h;
		h = target_h;
		w = lrint((float) w * m);
	}

	*crop_w = (target_w > 0 && w > target_w) ? lrint((float) (w - target_w) / m) : 0;
	*crop_h = (target_h > 0 && h > target_h) ? lrint((float) (h - target_h) / m) : 0;
}

void *MTY_CropImage(const void *image, uint32_t cropWidth, uint32_t cropHeight, uint32_t *width, uint32_t *height)
{
	uint32_t crop_width = 0;
	uint32_t crop_height = 0;
	image_center_crop(*width, *height, cropWidth, cropHeight, &crop_width, &crop_height);

	if (crop_width > 0 || crop_height > 0) {
		uint32_t x = crop_width / 2;
		uint32_t y = crop_height / 2;
		uint32_t crop_w = *width - crop_width;
		uint32_t crop_h = *height - crop_height;

		uint8_t *cropped = MTY_Alloc(crop_w * crop_h, 4);
		for (uint32_t h = y; h < *height - y && h - y < crop_h; h++)
			memcpy(cropped + ((h - y) * crop_w * 4), (uint8_t *) image + (h * *width * 4) + (x * 4), crop_w * 4);

		*width = crop_w;
		*height = crop_h;

		return cropped;
	}

	return NULL;
}
