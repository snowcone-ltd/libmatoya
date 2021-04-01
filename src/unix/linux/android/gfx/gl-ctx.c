// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include <string.h>

#include <EGL/egl.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "gfx/glproc.h"

static struct gl_ctx {
	ANativeWindow *window;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	MTY_Mutex *mutex;
	MTY_Renderer *renderer;
	bool ready;
	bool init;
	bool reinit;
	int32_t kb_height;
	uint32_t interval;
	uint32_t width;
	uint32_t height;
	uint32_t fb0;
	MTY_Atomic32 state_ctr;
} CTX;


// JNI

JNIEXPORT void JNICALL Java_group_matoya_lib_Matoya_gfx_1set_1surface(JNIEnv *env, jobject obj,
	jobject surface)
{
	MTY_MutexLock(CTX.mutex);

	CTX.window = ANativeWindow_fromSurface(env, surface);

	if (CTX.window) {
		CTX.ready = true;
		CTX.reinit = true;
	}

	MTY_MutexUnlock(CTX.mutex);
}

JNIEXPORT void JNICALL Java_group_matoya_lib_Matoya_gfx_1unset_1surface(JNIEnv *env, jobject obj)
{
	MTY_MutexLock(CTX.mutex);

	if (CTX.surface)
		eglDestroySurface(CTX.display, CTX.surface);

	if (CTX.window)
		ANativeWindow_release(CTX.window);

	CTX.surface = EGL_NO_SURFACE;
	CTX.window = NULL;

	CTX.ready = false;
	CTX.init = false;

	MTY_MutexUnlock(CTX.mutex);
}

void mty_gfx_global_init(void)
{
	CTX.mutex = MTY_MutexCreate();
}

void mty_gfx_global_destroy(void)
{
	MTY_MutexDestroy(&CTX.mutex);

	memset(&CTX, 0, sizeof(struct gl_ctx));
}

void mty_gfx_set_dims(uint32_t width, uint32_t height)
{
	CTX.width = width;
	CTX.height = height;

	MTY_Atomic32Set(&CTX.state_ctr, 2);
}

bool mty_gfx_is_ready(void)
{
	return CTX.ready;
}

void mty_gfx_size(uint32_t *width, uint32_t *height)
{
	*width = CTX.width;
	*height = CTX.height;
}

void mty_gfx_set_kb_height(int32_t height)
{
	CTX.kb_height = height;
}

MTY_ContextState mty_gfx_state(void)
{
	MTY_ContextState state = MTY_CONTEXT_STATE_NORMAL;

	MTY_MutexLock(CTX.mutex);

	if (CTX.reinit) {
		state = MTY_CONTEXT_STATE_NEW;
		CTX.reinit = false;

	} else if (MTY_Atomic32Get(&CTX.state_ctr) > 0) {
		MTY_Atomic32Add(&CTX.state_ctr, -1);
		state = MTY_CONTEXT_STATE_REFRESH;
	}

	MTY_MutexUnlock(CTX.mutex);

	return state;
}


// Private origin funcs for soft keyboard

void mty_gl_set_origin_y(int32_t y);
void mty_gl_ui_set_origin_y(int32_t y);


// EGL CTX

static void gl_ctx_create_context(struct gl_ctx *ctx)
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

	ctx->surface = eglCreateWindowSurface(ctx->display, config, ctx->window, NULL);

	const EGLint attrib[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, attrib);

	eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);
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
	if (!ctx->ready)
		return false;

	if (ctx->init)
		return true;

	MTY_RendererDestroy(&ctx->renderer);
	gl_ctx_destroy_context(ctx);

	gl_ctx_create_context(ctx);
	ctx->renderer = MTY_RendererCreate();

	ctx->init = true;

	return true;
}

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	return (struct gfx_ctx *) &CTX;
}

void mty_gl_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct gl_ctx *ctx = (struct gl_ctx *) *gfx_ctx;

	MTY_MutexLock(ctx->mutex);

	MTY_RendererDestroy(&ctx->renderer);
	gl_ctx_destroy_context(ctx);

	MTY_MutexUnlock(ctx->mutex);

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

void mty_gl_ctx_present(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	MTY_MutexLock(ctx->mutex);

	if (gl_ctx_check(ctx)) {
		if (ctx->interval != interval) {
			eglSwapInterval(ctx->display, interval);
			ctx->interval = interval;
		}

		eglSwapBuffers(ctx->display, ctx->surface);
		glFinish();
	}

	MTY_MutexUnlock(ctx->mutex);
}

void mty_gl_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	MTY_MutexLock(ctx->mutex);

	if (gl_ctx_check(ctx)) {
		MTY_RenderDesc mutated = *desc;
		mutated.viewWidth = ctx->width;
		mutated.viewHeight = ctx->height - ctx->kb_height;

		mty_gl_set_origin_y(ctx->kb_height);
		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_GL, NULL, NULL, image, &mutated, (MTY_Surface *) &ctx->fb0);
	}

	MTY_MutexUnlock(ctx->mutex);
}

void mty_gl_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	MTY_MutexLock(ctx->mutex);

	if (gl_ctx_check(ctx)) {
		mty_gl_ui_set_origin_y(ctx->kb_height);
		MTY_RendererDrawUI(ctx->renderer, MTY_GFX_GL, NULL, NULL, dd, (MTY_Surface *) &ctx->fb0);
	}

	MTY_MutexUnlock(ctx->mutex);
}

bool mty_gl_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	bool r = false;

	MTY_MutexLock(ctx->mutex);

	if (gl_ctx_check(ctx))
		r = MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_GL, NULL, NULL, id, rgba, width, height);

	MTY_MutexUnlock(ctx->mutex);

	return r;
}

bool mty_gl_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	bool r = false;

	MTY_MutexLock(ctx->mutex);

	if (gl_ctx_check(ctx))
		r = MTY_RendererHasUITexture(ctx->renderer, id);

	MTY_MutexUnlock(ctx->mutex);

	return r;
}

bool mty_gl_ctx_make_current(struct gfx_ctx *gfx_ctx, bool current)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	bool r = false;

	MTY_MutexLock(ctx->mutex);

	if (gl_ctx_check(ctx)) {
		if (current) {
			r = eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context);

		} else {
			r = eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		}
	}

	MTY_MutexUnlock(ctx->mutex);

	return r;
}
