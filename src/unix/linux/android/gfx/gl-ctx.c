// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include <string.h>

#include <EGL/egl.h>

#include "app.h"
#include "gfx/gl/glproc.h"

static struct gl_ctx {
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	MTY_Renderer *renderer;
	bool init;
	bool vsync;
	uint32_t fb0;
} CTX;


// Private origin funcs for soft keyboard

void mty_gl_set_origin_y(int32_t y);
void mty_gl_ui_set_origin_y(int32_t y);


// EGL CTX

void mty_gl_ctx_destroy_egl_surface(void)
{
	if (CTX.surface)
		eglDestroySurface(CTX.display, CTX.surface);

	CTX.init = false;
	CTX.surface = EGL_NO_SURFACE;
}

void mty_gl_ctx_global_destroy(void)
{
	memset(&CTX, 0, sizeof(struct gl_ctx));
}

static void gl_ctx_create_context(struct gl_ctx *ctx, NativeWindowType window)
{
	ctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(ctx->display, NULL, NULL);

	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
	};

	EGLint n = 0;
	EGLConfig config = NULL;
	eglChooseConfig(ctx->display, attribs, &config, 1, &n);

	ctx->surface = eglCreateWindowSurface(ctx->display, config, window, NULL);

	const EGLint attrib[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, attrib);

	eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);

	eglSwapInterval(ctx->display, CTX.vsync ? 1 : 0);
}

static void gl_ctx_destroy_context(struct gl_ctx *ctx)
{
	if (ctx->display) {
		if (ctx->context) {
			eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			eglDestroyContext(ctx->display, ctx->context);
		}

		if (ctx->surface)
			eglDestroySurface(ctx->display, ctx->surface);

		eglTerminate(ctx->display);
	}

	ctx->display = EGL_NO_DISPLAY;
	ctx->surface = EGL_NO_SURFACE;
	ctx->context = EGL_NO_CONTEXT;
}


// GFX

static bool gl_ctx_check(struct gl_ctx *ctx)
{
	NativeWindowType window = MTY_WindowGetNative(NULL, 0);
	if (!window)
		return false;

	if (ctx->init)
		return true;

	MTY_RendererDestroy(&ctx->renderer);
	gl_ctx_destroy_context(ctx);

	gl_ctx_create_context(ctx, window);
	ctx->renderer = MTY_RendererCreate();

	ctx->init = true;

	return true;
}

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	CTX.vsync = vsync;

	return (struct gfx_ctx *) &CTX;
}

void mty_gl_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct gl_ctx *ctx = (struct gl_ctx *) *gfx_ctx;

	mty_gfx_lock();

	MTY_RendererDestroy(&ctx->renderer);
	gl_ctx_destroy_context(ctx);

	mty_gfx_unlock();

	*gfx_ctx = NULL;
}

MTY_Device *mty_gl_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	return NULL;
}

MTY_Context *mty_gl_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return (MTY_Context *) ctx->context;
}

MTY_Surface *mty_gl_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return (MTY_Surface *) &ctx->fb0;
}

void mty_gl_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	mty_gfx_lock();

	if (gl_ctx_check(ctx)) {
		eglSwapBuffers(ctx->display, ctx->surface);
		glFinish();
	}

	mty_gfx_unlock();
}

void mty_gl_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	mty_gfx_lock();

	if (gl_ctx_check(ctx)) {
		MTY_RenderDesc mutated = *desc;
		mty_gfx_size(&mutated.viewWidth, &mutated.viewHeight);

		int32_t kb_height = mty_app_get_kb_height();
		mutated.viewHeight -= kb_height;

		mty_gl_set_origin_y(kb_height);
		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_GL, NULL, NULL, image, &mutated, (MTY_Surface *) &ctx->fb0);
	}

	mty_gfx_unlock();
}

void mty_gl_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	mty_gfx_lock();

	if (gl_ctx_check(ctx)) {
		int32_t kb_height = mty_app_get_kb_height();
		mty_gl_ui_set_origin_y(kb_height);

		MTY_RendererDrawUI(ctx->renderer, MTY_GFX_GL, NULL, NULL, dd, (MTY_Surface *) &ctx->fb0);
	}

	mty_gfx_unlock();
}

bool mty_gl_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	bool r = false;

	mty_gfx_lock();

	if (gl_ctx_check(ctx))
		r = MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_GL, NULL, NULL, id, rgba, width, height);

	mty_gfx_unlock();

	return r;
}

bool mty_gl_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	bool r = false;

	mty_gfx_lock();

	if (gl_ctx_check(ctx))
		r = MTY_RendererHasUITexture(ctx->renderer, id);

	mty_gfx_unlock();

	return r;
}
