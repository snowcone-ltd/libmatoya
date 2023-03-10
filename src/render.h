// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

struct renderer;

struct renderer *mty_renderer_create(void);
void mty_renderer_destroy(struct renderer **renderer);
bool mty_renderer_draw_quad(struct renderer *ctx, MTY_GFX api, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dst);
void mty_renderer_clear(struct renderer *ctx, MTY_GFX api, MTY_Device *device, MTY_Context *context,
	uint32_t width, uint32_t height, float r, float g, float b, float a, MTY_Surface *dst);
bool mty_renderer_draw_ui(struct renderer *ctx, MTY_GFX api, MTY_Device *device,
	MTY_Context *context, const MTY_DrawData *dd, MTY_Surface *dst);
bool mty_renderer_set_ui_texture(struct renderer *ctx, MTY_GFX api, MTY_Device *device,
	MTY_Context *context, uint32_t id, const void *rgba, uint32_t width, uint32_t height);
bool mty_renderer_has_ui_texture(struct renderer *ctx, uint32_t id);