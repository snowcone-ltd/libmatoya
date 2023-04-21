// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include "../web.h"

struct gl_ctx {
	MTY_App *app;
	uint32_t fb0;
};

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	struct gl_ctx *ctx = MTY_Alloc(1, sizeof(struct gl_ctx));

	ctx->app = native_window;

	web_set_gfx();

	return (struct gfx_ctx *) ctx;
}

void mty_gl_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct gl_ctx *ctx = (struct gl_ctx *) *gfx_ctx;

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

void mty_gl_ctx_get_size(struct gfx_ctx *gfx_ctx, uint32_t *w, uint32_t *h)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	MTY_Size size = MTY_WindowGetSize(ctx->app, 0);
	*w = size.w;
	*h = size.h;

	web_set_canvas_size(size.w, size.h);
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

bool mty_gl_ctx_lock(struct gfx_ctx *gfx_ctx)
{
	return true;
}

void mty_gl_ctx_unlock(void)
{
}
