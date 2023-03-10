// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

#include <stdint.h>
#include <stdbool.h>

#include "gfx/mod-support.h"
#include "gfx/mod-common.h"

// https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
struct gfx_uniforms {
	float width;
	float height;
	float vp_height;
	float pad0;
	uint32_t effects[4];
	float levels[4];
	uint32_t planes;
	uint32_t rotation;
	uint32_t conversion;
	uint32_t pad1;
};

struct gfx;

#define GFX_PROTO(api, name) mty##api##name
#define GFX_FP(api, name)    (*name)

#define GFX_DECLARE_API(api, wrap) \
	struct gfx *wrap(api, create)(struct gfx_device *device, uint8_t layer); \
	void wrap(api, destroy)(struct gfx **gfx, struct gfx_device *device); \
	bool wrap(api, render)(struct gfx *gfx, struct gfx_device *device, struct gfx_context *context, \
		const void *image, const MTY_RenderDesc *desc, struct gfx_surface *dest); \
	void wrap(api, clear)(struct gfx *gfx, struct gfx_device *device, struct gfx_context *context, \
		uint32_t width, uint32_t height, float r, float g, float b, float a, struct gfx_surface *dest);

#define GFX_PROTOTYPES(api) \
	GFX_DECLARE_API(api, GFX_PROTO)

#define GFX_DECLARE_ROW(API, api) \
	[MTY_GFX_##API] = { \
		mty##api##create, \
		mty##api##destroy, \
		mty##api##render, \
		mty##api##clear, \
	},
