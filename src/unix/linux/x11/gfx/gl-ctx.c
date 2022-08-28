// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include "gfx/gl/glproc.c"
#include "dl/libx11.c"

struct gl_ctx {
	Display *display;
	XVisualInfo *vis;
	Window window;
	GLXContext gl;
	MTY_Renderer *renderer;
	uint32_t fb0;
};

static void gl_ctx_get_size(struct gl_ctx *ctx, uint32_t *width, uint32_t *height)
{
	XWindowAttributes attr = {0};
	XGetWindowAttributes(ctx->display, ctx->window, &attr);

	*width = attr.width;
	*height = attr.height;
}

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	if (!libX11_global_init())
		return NULL;

	if (!glproc_global_init())
		return NULL;

	bool r = true;

	struct gl_ctx *ctx = MTY_Alloc(1, sizeof(struct gl_ctx));
	struct xinfo *info = (struct xinfo *) native_window;
	ctx->display = info->display;
	ctx->vis = info->vis;
	ctx->window = info->window;
	ctx->renderer = MTY_RendererCreate();

	ctx->gl = glXCreateContext(ctx->display, ctx->vis, NULL, GL_TRUE);
	if (!ctx->gl) {
		r = false;
		MTY_Log("'glXCreateContext' failed");
		goto except;
	}

	glXMakeCurrent(ctx->display, ctx->window, ctx->gl);

	void (*glXSwapIntervalEXT)(Display *dpy, GLXDrawable drawable, int interval)
		= glXGetProcAddress((const unsigned char *) "glXSwapIntervalEXT");

	if (glXSwapIntervalEXT)
		glXSwapIntervalEXT(ctx->display, ctx->window, vsync ? 1 : 0);

	except:

	if (!r)
		mty_gl_ctx_destroy((struct gfx_ctx **) &ctx);

	return (struct gfx_ctx *) ctx;
}

void mty_gl_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct gl_ctx *ctx = (struct gl_ctx *) *gfx_ctx;

	MTY_RendererDestroy(&ctx->renderer);

	if (ctx->gl)
		glXDestroyContext(ctx->display, ctx->gl);

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

MTY_Device *mty_gl_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	return NULL;
}

MTY_Context *mty_gl_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return (MTY_Context *) ctx->gl;
}

MTY_Surface *mty_gl_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return (MTY_Surface *) &ctx->fb0;
}

void mty_gl_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	glXSwapBuffers(ctx->display, ctx->window);
	glFinish();
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
