// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "render.h"

#include "gfx/mod.h"
#include "gfx/mod-ui.h"

#define RENDER_LAYERS 2

GFX_PROTOTYPES(_gl_)
GFX_PROTOTYPES(_vk_)
GFX_PROTOTYPES(_d3d11_)
GFX_PROTOTYPES(_d3d12_)
GFX_PROTOTYPES(_metal_)
GFX_DECLARE_TABLE()

GFX_UI_PROTOTYPES(_gl_)
GFX_UI_PROTOTYPES(_vk_)
GFX_UI_PROTOTYPES(_d3d11_)
GFX_UI_PROTOTYPES(_d3d12_)
GFX_UI_PROTOTYPES(_metal_)
GFX_UI_DECLARE_TABLE()

struct renderer {
	MTY_GFX api;
	struct gfx_device *device;
	MTY_Hash *textures;

	struct gfx *gfx[RENDER_LAYERS];
	struct gfx_ui *gfx_ui;
};

struct renderer *mty_renderer_create(MTY_GFX api)
{
	struct renderer *ctx = MTY_Alloc(1, sizeof(struct renderer));
	ctx->textures = MTY_HashCreate(0);
	ctx->api = api;

	return ctx;
}

static void render_destroy_device(struct renderer *ctx)
{
	for (uint8_t x = 0; x < RENDER_LAYERS; x++)
		GFX_API[ctx->api].destroy(&ctx->gfx[x], ctx->device);

	for (int64_t id = 0, i = 0; MTY_HashGetNextKeyInt(ctx->textures, (uint64_t *) &i, &id);) {
		void *texture = MTY_HashPopInt(ctx->textures, id);
		GFX_UI_API[ctx->api].destroy_texture(ctx->gfx_ui, &texture, ctx->device);
	}

	GFX_UI_API[ctx->api].destroy(&ctx->gfx_ui, ctx->device);

	ctx->device = NULL;
}

void mty_renderer_destroy(struct renderer **renderer)
{
	if (!renderer || !*renderer)
		return;

	struct renderer *ctx = *renderer;

	render_destroy_device(ctx);
	MTY_HashDestroy(&ctx->textures, NULL);

	MTY_Free(ctx);
	*renderer = NULL;
}

static void render_set_device(struct renderer *ctx, struct gfx_device *device)
{
	if (ctx->device != device) {
		render_destroy_device(ctx);
		ctx->device = device;
	}
}

static bool renderer_begin(struct renderer *ctx, uint8_t layer, struct gfx_context *context, struct gfx_device *device)
{
	if (layer >= RENDER_LAYERS)
		return false;

	render_set_device(ctx, device);

	if (!ctx->gfx[layer])
		ctx->gfx[layer] = GFX_API[ctx->api].create(device, layer);

	return ctx->gfx[layer] != NULL;
}

static bool renderer_begin_ui(struct renderer *ctx, struct gfx_context *context, struct gfx_device *device)
{
	render_set_device(ctx, device);

	if (!ctx->gfx_ui)
		ctx->gfx_ui = GFX_UI_API[ctx->api].create(device);

	return ctx->gfx_ui != NULL;
}

bool mty_renderer_draw_quad(struct renderer *ctx, struct gfx_device *device, struct gfx_context *context,
	const void *image, const MTY_RenderDesc *desc, struct gfx_surface *dest)
{
	if (!renderer_begin(ctx, desc->layer, context, device))
		return false;

	return GFX_API[ctx->api].render(ctx->gfx[desc->layer], device, context, image, desc, dest);
}

void mty_renderer_clear(struct renderer *ctx, struct gfx_device *device, struct gfx_context *context,
	uint32_t width, uint32_t height, float r, float g, float b, float a, struct gfx_surface *dest)
{
	if (!renderer_begin(ctx, 0, context, device))
		return;

	GFX_API[ctx->api].clear(ctx->gfx[0], device, context, width, height, r, g, b, a, dest);
}

bool mty_renderer_draw_ui(struct renderer *ctx, struct gfx_device *device,
	struct gfx_context *context, const MTY_DrawData *dd, struct gfx_surface *dest)
{
	if (!renderer_begin_ui(ctx, context, device))
		return false;

	return GFX_UI_API[ctx->api].render(ctx->gfx_ui, device, context, dd, ctx->textures, dest);
}

bool mty_renderer_set_ui_texture(struct renderer *ctx, struct gfx_device *device,
	struct gfx_context *context, uint32_t id, const void *rgba, uint32_t width, uint32_t height)
{
	if (!renderer_begin_ui(ctx, context, device))
		return false;

	void *texture = MTY_HashPopInt(ctx->textures, id);
	if (texture)
		GFX_UI_API[ctx->api].destroy_texture(ctx->gfx_ui, &texture, ctx->device);

	if (rgba) {
		texture = GFX_UI_API[ctx->api].create_texture(ctx->gfx_ui, device, rgba, width, height);
		MTY_HashSetInt(ctx->textures, id, texture);
	}

	return texture != NULL;
}

bool mty_renderer_has_ui_texture(struct renderer *ctx, uint32_t id)
{
	return MTY_HashGetInt(ctx->textures, id) != NULL;
}
