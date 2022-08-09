// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include "../web.h"

struct gl_ctx {
	MTY_Renderer *renderer;
	uint32_t fb0;
};

static void gl_ctx_get_size(struct gl_ctx *ctx, uint32_t *width, uint32_t *height)
{
	web_get_size(width, height);
}

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	struct gl_ctx *ctx = MTY_Alloc(1, sizeof(struct gl_ctx));
	ctx->renderer = MTY_RendererCreate();

	return (struct gfx_ctx *) ctx;
}

void mty_gl_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct gl_ctx *ctx = (struct gl_ctx *) *gfx_ctx;

	MTY_RendererDestroy(&ctx->renderer);

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

MTY_Device *mty_gl_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	return NULL;
}

MTY_Context *mty_gl_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	return NULL;
}

MTY_Surface *mty_gl_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return (MTY_Surface *) &ctx->fb0;
}

void mty_gl_ctx_present(struct gfx_ctx *gfx_ctx)
{
	// This helps jitter
	web_gl_flush();
}

void mty_gl_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	MTY_RenderDesc mutated = *desc;
	gl_ctx_get_size(ctx, &mutated.viewWidth, &mutated.viewHeight);

	MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_GL, NULL, NULL, image, &mutated, (MTY_Surface *) &ctx->fb0);
}

void mty_gl_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	MTY_RendererDrawUI(ctx->renderer, MTY_GFX_GL, NULL, NULL, dd, (MTY_Surface *) &ctx->fb0);
}

bool mty_gl_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_GL, NULL, NULL, id, rgba, width, height);
}

bool mty_gl_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return MTY_RendererHasUITexture(ctx->renderer, id);
}
