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
	uint32_t fb0;
};

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

	if (ctx->gl)
		glXDestroyContext(ctx->display, ctx->gl);

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

void mty_gl_ctx_get_size(struct gfx_ctx *gfx_ctx, uint32_t *w, uint32_t *h)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	XWindowAttributes attr = {0};
	XGetWindowAttributes(ctx->display, ctx->window, &attr);

	*w = attr.width;
	*h = attr.height;
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

void mty_gl_ctx_set_sync_interval(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
}

void mty_gl_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	glXSwapBuffers(ctx->display, ctx->window);
	glFinish();
}

bool mty_gl_ctx_lock(struct gfx_ctx *gfx_ctx)
{
	return true;
}

void mty_gl_ctx_unlock(void)
{
}
