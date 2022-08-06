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

struct gfx;

#define GFX_PROTO(api, name) mty##api##name
#define GFX_FP(api, name)    (*name)

#define GFX_DECLARE_API(api, wrap) \
	struct gfx *wrap(api, create)(MTY_Device *device); \
	void wrap(api, destroy)(struct gfx **gfx, MTY_Device *device); \
	bool wrap(api, render)(struct gfx *gfx, MTY_Device *device, MTY_Context *context, \
		const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest); \
	void *wrap(api, get_state)(MTY_Device *device, MTY_Context *context); \
	void wrap(api, set_state)(MTY_Device *device, MTY_Context *context, void *state); \
	void wrap(api, free_state)(void **state);

#define GFX_PROTOTYPES(api) \
	GFX_DECLARE_API(api, GFX_PROTO)

#define GFX_DECLARE_ROW(API, api) \
	[MTY_GFX_##API] = { \
		mty##api##create, \
		mty##api##destroy, \
		mty##api##render, \
		mty##api##get_state, \
		mty##api##set_state, \
		mty##api##free_state, \
	},
