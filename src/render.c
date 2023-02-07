// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "gfx/mod.h"
#include "gfx/mod-ui.h"

GFX_PROTOTYPES(_gl_)
GFX_PROTOTYPES(_vk_)
GFX_PROTOTYPES(_d3d9_)
GFX_PROTOTYPES(_d3d11_)
GFX_PROTOTYPES(_d3d12_)
GFX_PROTOTYPES(_metal_)
GFX_DECLARE_TABLE()

GFX_UI_PROTOTYPES(_gl_)
GFX_UI_PROTOTYPES(_vk_)
GFX_UI_PROTOTYPES(_d3d9_)
GFX_UI_PROTOTYPES(_d3d11_)
GFX_UI_PROTOTYPES(_d3d12_)
GFX_UI_PROTOTYPES(_metal_)
GFX_UI_DECLARE_TABLE()

struct MTY_Renderer {
	MTY_GFX api;
	MTY_Device *device;
	MTY_Hash *textures;

	struct gfx *gfx;
	struct gfx_ui *gfx_ui;
};

struct MTY_RenderState {
	MTY_GFX api;
	void *opaque;
};

MTY_Renderer *MTY_RendererCreate(void)
{
	MTY_Renderer *ctx = MTY_Alloc(1, sizeof(MTY_Renderer));
	ctx->textures = MTY_HashCreate(0);

	return ctx;
}

static void render_destroy_api(MTY_Renderer *ctx)
{
	if (ctx->api == MTY_GFX_NONE)
		return;

	GFX_API[ctx->api].destroy(&ctx->gfx, ctx->device);

	uint64_t i = 0;
	int64_t id = 0;
	while (MTY_HashGetNextKeyInt(ctx->textures, &i, &id)) {
		void *texture = MTY_HashPopInt(ctx->textures, id);
		GFX_UI_API[ctx->api].destroy_texture(ctx->gfx_ui, &texture, ctx->device);
	}

	GFX_UI_API[ctx->api].destroy(&ctx->gfx_ui, ctx->device);

	ctx->api = MTY_GFX_NONE;
	ctx->device = NULL;
}

void MTY_RendererDestroy(MTY_Renderer **renderer)
{
	if (!renderer || !*renderer)
		return;

	MTY_Renderer *ctx = *renderer;

	render_destroy_api(ctx);
	MTY_HashDestroy(&ctx->textures, NULL);

	MTY_Free(ctx);
	*renderer = NULL;
}

static bool render_create_api(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device)
{
	ctx->api = api;
	ctx->device = device;

	ctx->gfx = GFX_API[api].create(device);
	ctx->gfx_ui = GFX_UI_API[api].create(device);

	if (!ctx->gfx || !ctx->gfx_ui) {
		render_destroy_api(ctx);
		return false;
	}

	return true;
}

static bool renderer_begin(MTY_Renderer *ctx, MTY_GFX api, MTY_Context *context, MTY_Device *device)
{
	if (ctx->api != api || ctx->device != device)
		render_destroy_api(ctx);

	if (!ctx->gfx || !ctx->gfx_ui)
		return render_create_api(ctx, api, device);

	return true;
}

bool MTY_RendererDrawQuad(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dst)
{
	if (!renderer_begin(ctx, api, context, device))
		return false;

	return GFX_API[api].render(ctx->gfx, device, context, image, desc, dst);
}

bool MTY_RendererDrawUI(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device,
	MTY_Context *context, const MTY_DrawData *dd, MTY_Surface *dst)
{
	if (!renderer_begin(ctx, api, context, device))
		return false;

	return GFX_UI_API[api].render(ctx->gfx_ui, device, context, dd, ctx->textures, dst);
}

bool MTY_RendererSetUITexture(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device,
	MTY_Context *context, uint32_t id, const void *rgba, uint32_t width, uint32_t height)
{
	if (!renderer_begin(ctx, api, context, device))
		return false;

	void *texture = MTY_HashPopInt(ctx->textures, id);
	if (texture)
		GFX_UI_API[api].destroy_texture(ctx->gfx_ui, &texture, ctx->device);

	if (rgba) {
		texture = GFX_UI_API[api].create_texture(ctx->gfx_ui, device, rgba, width, height);
		MTY_HashSetInt(ctx->textures, id, texture);
	}

	return texture != NULL;
}

bool MTY_RendererHasUITexture(MTY_Renderer *ctx, uint32_t id)
{
	return MTY_HashGetInt(ctx->textures, id) != NULL;
}

uint32_t MTY_GetAvailableGFX(MTY_GFX *apis)
{
	uint32_t r = 0;

	for (uint32_t x = 0; x < MTY_GFX_MAX; x++)
		if (GFX_API_SUPPORTED(x))
			apis[r++] = x;

	return r;
}

MTY_GFX MTY_GetDefaultGFX(void)
{
	return GFX_API_DEFAULT;
}

MTY_RenderState *MTY_GetRenderState(MTY_GFX api, MTY_Device *device, MTY_Context *context)
{
	MTY_RenderState *state = MTY_Alloc(1, sizeof(MTY_RenderState));

	state->api = api;
	state->opaque = GFX_API[api].get_state(device, context);

	return state;
}

void MTY_SetRenderState(MTY_GFX api, MTY_Device *device, MTY_Context *context,
	MTY_RenderState *state)
{
	if (!state || !state->opaque || state->api != api)
		return;

	GFX_API[api].set_state(device, context, state->opaque);
}

void MTY_FreeRenderState(MTY_RenderState **state)
{
	if (!state || !*state)
		return;

	MTY_RenderState *s = *state;

	GFX_API[s->api].free_state(&s->opaque);

	MTY_Free(s);
	*state = NULL;
}
