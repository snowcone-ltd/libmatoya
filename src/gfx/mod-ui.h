// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

#include <stdint.h>
#include <stdbool.h>

#include "gfx/mod-support.h"
#include "gfx/mod-common.h"

struct gfx_ui;

#define GFX_UI_VTX_INCR (1024 * 5)
#define GFX_UI_IDX_INCR (1024 * 10)

#define GFX_UI_PROTO(api, name) mty##api##ui_##name
#define GFX_UI_FP(api, name)    (*name)

#define GFX_UI_DECLARE_API(api, wrap) \
	struct gfx_ui *wrap(api, create)(struct gfx_device *device); \
	void wrap(api, destroy)(struct gfx_ui **gfx_ui, struct gfx_device *device); \
	bool wrap(api, render)(struct gfx_ui *gfx_ui, struct gfx_device *device, \
		struct gfx_context *context, const MTY_DrawData *dd, MTY_Hash *cache, \
		struct gfx_surface *dest); \
	void *wrap(api, create_texture)(struct gfx_ui *gfx_ui, struct gfx_device *device, \
		const void *rgba, uint32_t width, uint32_t height); \
	void wrap(api, destroy_texture)(struct gfx_ui *gfx_ui, void **texture, \
		struct gfx_device *device);

#define GFX_UI_PROTOTYPES(api) \
	GFX_UI_DECLARE_API(api, GFX_UI_PROTO)

#define GFX_UI_DECLARE_ROW(API, api) \
	[MTY_GFX_##API] = { \
		mty##api##ui_create, \
		mty##api##ui_destroy, \
		mty##api##ui_render, \
		mty##api##ui_create_texture, \
		mty##api##ui_destroy_texture, \
	},
