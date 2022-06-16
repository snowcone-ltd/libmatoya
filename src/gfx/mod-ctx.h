// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

#include <stdint.h>
#include <stdbool.h>

#include "gfx/mod-support.h"

struct gfx_ctx;

#define GFX_CTX_PROTO(api, name) mty##api##ctx_##name
#define GFX_CTX_FP(api, name)    (*name)

#define GFX_CTX_DECLARE_API(api, wrap) \
	struct gfx_ctx *wrap(api, create)(void *native_window, bool vsync); \
	void wrap(api, destroy)(struct gfx_ctx **gfx_ctx); \
	void wrap(api, present)(struct gfx_ctx *gfx_ctx, uint32_t num_frames); \
	MTY_Device *wrap(api, get_device)(struct gfx_ctx *gfx_ctx); \
	MTY_Context *wrap(api, get_context)(struct gfx_ctx *gfx_ctx); \
	MTY_Surface *wrap(api, get_surface)(struct gfx_ctx *gfx_ctx); \
	void wrap(api, draw_quad)(struct gfx_ctx *gfx_ctx, const void *image, \
		const MTY_RenderDesc *desc); \
	void wrap(api, draw_ui)(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd); \
	bool wrap(api, set_ui_texture)(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba, \
		uint32_t width, uint32_t height); \
	bool wrap(api, has_ui_texture)(struct gfx_ctx *gfx_ctx, uint32_t id); \
	bool wrap(api, make_current)(struct gfx_ctx *gfx_ctx, bool current); \
	bool wrap(api, hdr_supported)(struct gfx_ctx *gfx_ctx);

#define GFX_CTX_PROTOTYPES(api) \
	GFX_CTX_DECLARE_API(api, GFX_CTX_PROTO)

#define GFX_CTX_DECLARE_ROW(API, api) \
	[MTY_GFX_##API] = { \
		mty##api##ctx_create, \
		mty##api##ctx_destroy, \
		mty##api##ctx_present, \
		mty##api##ctx_get_device, \
		mty##api##ctx_get_context, \
		mty##api##ctx_get_surface, \
		mty##api##ctx_draw_quad, \
		mty##api##ctx_draw_ui, \
		mty##api##ctx_set_ui_texture, \
		mty##api##ctx_has_ui_texture, \
		mty##api##ctx_make_current, \
		mty##api##ctx_hdr_supported, \
	},
