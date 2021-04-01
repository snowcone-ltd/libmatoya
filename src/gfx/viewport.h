// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <math.h>

static void mty_viewport(MTY_Rotation rotation, uint32_t w, uint32_t h,
	uint32_t view_w, uint32_t view_h, float ar, float scale, float *vp_x,
	float *vp_y, float *vp_w, float *vp_h)
{
	if (rotation == MTY_ROTATION_90 || rotation == MTY_ROTATION_270) {
		uint32_t tmp = h;
		h = w;
		w = tmp;
		ar = 1.0f / ar;
	}

	uint32_t scaled_w = lrint(scale * (float) w);
	uint32_t scaled_h = lrint(scale * (float) h);

	if (scaled_w == 0 || scaled_h == 0 || view_w < scaled_w || view_h < scaled_h)
		scaled_w = view_w;

	*vp_w = (float) scaled_w;
	*vp_h = roundf(*vp_w / ar);

	if (*vp_w > (float) view_w) {
		*vp_w = (float) view_w;
		*vp_h = roundf(*vp_w / ar);
	}

	if (*vp_h > (float) view_h) {
		*vp_h = (float) view_h;
		*vp_w = roundf(*vp_h * ar);
	}

	*vp_x = roundf(((float) view_w - *vp_w) / 2.0f);
	*vp_y = roundf(((float) view_h - *vp_h) / 2.0f);
}
