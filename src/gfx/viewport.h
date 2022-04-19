// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <math.h>

static void mty_viewport(const MTY_RenderDesc *desc, float *vp_x, float *vp_y, float *vp_w, float *vp_h, bool transform_origin)
{
	float w      = (float) desc->cropWidth;
	float h      = (float) desc->cropHeight;
	float view_w = (float) desc->viewWidth;
	float view_h = (float) desc->viewHeight;
	float ar     = desc->aspectRatio;

	if (desc->rotation == MTY_ROTATION_90 || desc->rotation == MTY_ROTATION_270) {
		float tmp = h;
		h = w;
		w = tmp;
		ar = 1.0f / ar;
	}

	float scaled_w = roundf(desc->scale * w);
	float scaled_h = roundf(desc->scale * h);

	switch (desc->type)	{
		default:
		case MTY_POSITION_AUTO:
			if (scaled_w == 0 || scaled_h == 0 || view_w < scaled_w || view_h < scaled_h)
				scaled_w = view_w;

			*vp_w = scaled_w;
			*vp_h = roundf(*vp_w / ar);

			if (*vp_w > view_w) {
				*vp_w = view_w;
				*vp_h = roundf(*vp_w / ar);
			}

			if (*vp_h > view_h) {
				*vp_h = view_h;
				*vp_w = roundf(*vp_h * ar);
			}

			*vp_x = (view_w - *vp_w) / 2.0f;
			*vp_y = (view_h - *vp_h) / 2.0f;
			break;
		case MTY_POSITION_FIXED:
			*vp_w = scaled_w;
			*vp_h = scaled_h;

			*vp_x = (float)desc->imageX;
			*vp_y = (float)desc->imageY;

			if (transform_origin)
				*vp_y = desc->displayHeight - scaled_h - *vp_y;
			break;
	}

	*vp_x = roundf(*vp_x);
	*vp_y = roundf(*vp_y);
}
