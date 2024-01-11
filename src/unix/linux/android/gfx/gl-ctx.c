// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include <string.h>

#include <EGL/egl.h>

#include "app-os.h"
#include "gfx/gl/glproc.h"

static struct gl_ctx {
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	bool init;
	bool vsync;
	uint32_t fb0;
	uintptr_t device;
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

	gl_ctx_destroy_context(ctx);
	gl_ctx_create_context(ctx, window);

	ctx->device++; // This will force an MTY_Renderer reinit
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

	gl_ctx_destroy_context(ctx);

	mty_gfx_unlock();

	*gfx_ctx = NULL;
}

void mty_gl_ctx_get_size(struct gfx_ctx *gfx_ctx, uint32_t *w, uint32_t *h)
{
	mty_gfx_size(w, h);

	uint32_t kb_height = mty_app_get_kb_height();

	if (*h > kb_height)
		*h -= kb_height;

	mty_gl_set_origin_y(kb_height);
	mty_gl_ui_set_origin_y(kb_height);
}

MTY_Device *mty_gl_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return (MTY_Device *) ctx->device;
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

void mty_gl_ctx_set_sync_interval(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
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

bool mty_gl_ctx_lock(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	mty_gfx_lock();

	if (!gl_ctx_check(ctx)) {
		mty_gfx_unlock();
		return false;
	}

	return true;
}

void mty_gl_ctx_unlock(void)
{
	mty_gfx_unlock();
}
