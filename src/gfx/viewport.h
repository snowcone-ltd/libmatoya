// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <math.h>

static void mty_viewport(const MTY_RenderDesc *desc, float *vp_x, float *vp_y, float *vp_w, float *vp_h)
{
	uint32_t w = desc->cropWidth;
	uint32_t h = desc->cropHeight;
	float ar = desc->aspectRatio > 0 ? desc->aspectRatio : h > 0 ? (float) w / h : 1;

	if (desc->rotation == MTY_ROTATION_90 || desc->rotation == MTY_ROTATION_270) {
		uint32_t tmp = h;
		h = w;
		w = tmp;
		ar = 1 / ar;
	}

	uint32_t scaled_w = lrint(desc->scale * w);
	uint32_t scaled_h = lrint(desc->scale * h);

	if (scaled_w == 0 || scaled_h == 0 || desc->viewWidth < scaled_w || desc->viewHeight < scaled_h)
		scaled_w = desc->viewWidth;

	*vp_w = (float) scaled_w;
	*vp_h = roundf(*vp_w / ar);

	if (*vp_w > (float) desc->viewWidth) {
		*vp_w = (float) desc->viewWidth;
		*vp_h = roundf(*vp_w / ar);
	}

	if (*vp_h > (float) desc->viewHeight) {
		*vp_h = (float) desc->viewHeight;
		*vp_w = roundf(*vp_h * ar);
	}

	*vp_x = roundf(((float) desc->viewWidth - *vp_w) / 2);
	*vp_y = roundf(((float) desc->viewHeight - *vp_h) / 2);
}
