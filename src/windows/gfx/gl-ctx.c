// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include <windows.h>
#include <gl/gl.h>

struct gl_ctx {
	HWND hwnd;
	HGLRC gl;
	HDC dc;
	MTY_Renderer *renderer;
	BOOL (WINAPI *wglSwapIntervalEXT)(int inverval);
	uint32_t interval;
	uint32_t width;
	uint32_t height;
	uint32_t fb0;
};

static void gl_ctx_get_size(struct gl_ctx *ctx, uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	GetClientRect(ctx->hwnd, &rect);

	*width = rect.right - rect.left;
	*height = rect.bottom - rect.top;
}

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	struct gl_ctx *ctx = MTY_Alloc(1, sizeof(struct gl_ctx));
	ctx->hwnd = (HWND) native_window;
	ctx->renderer = MTY_RendererCreate();

	bool r = true;

	PIXELFORMATDESCRIPTOR pfd = {0};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DEPTH_DONTCARE |
		PFD_DOUBLEBUFFER | PFD_SWAP_LAYER_BUFFERS | PFD_SWAP_EXCHANGE;
	pfd.iPixelType = PFD_TYPE_RGBA;

	ctx->dc = GetDC(ctx->hwnd);
	if (!ctx->dc) {
		r = false;
		MTY_Log("'GetDC' failed");
		goto except;
	}

	int32_t pf = ChoosePixelFormat(ctx->dc, &pfd);
	if (pf == 0) {
		r = false;
		MTY_Log("'ChoosePixelFormat' failed with error 0x%X", GetLastError());
		goto except;
	}

	if (!SetPixelFormat(ctx->dc, pf, &pfd)) {
		r = false;
		MTY_Log("'SetPixelFormat' failed with error 0x%X", GetLastError());
		goto except;
	}

	ctx->gl = wglCreateContext(ctx->dc);
	if (!ctx->gl) {
		r = false;
		MTY_Log("'wglCreateContext' failed with error 0x%X", GetLastError());
		goto except;
	}

	if (!wglMakeCurrent(ctx->dc, ctx->gl)) {
		r = false;
		MTY_Log("'wglMakeCurrent' failed with error 0x%X", GetLastError());
		goto except;
	}

	ctx->wglSwapIntervalEXT = (void *) wglGetProcAddress("wglSwapIntervalEXT");
	if (!ctx->wglSwapIntervalEXT)
		MTY_Log("'wglGetProcAddress' failed to find 'wglSwapIntervalEXT'");

	wglMakeCurrent(NULL, NULL);

	gl_ctx_get_size(ctx, &ctx->width, &ctx->height);

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

	wglMakeCurrent(NULL, NULL);

	if (ctx->gl)
		wglDeleteContext(ctx->gl);

	if (ctx->hwnd && ctx->dc)
		ReleaseDC(ctx->hwnd, ctx->dc);

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

void mty_gl_ctx_present(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	if (ctx->wglSwapIntervalEXT && interval != ctx->interval) {
		if (!ctx->wglSwapIntervalEXT(interval))
			MTY_Log("'wglSwapIntervalEXT' failed with error 0x%X", GetLastError());

		ctx->interval = interval;
	}

	if (!wglSwapLayerBuffers(ctx->dc, WGL_SWAP_MAIN_PLANE))
		MTY_Log("'wglSwapLayerBuffers' failed with error 0x%X", GetLastError());

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

bool mty_gl_ctx_make_current(struct gfx_ctx *gfx_ctx, bool current)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	bool r = false;

	if (current) {
		r = wglMakeCurrent(ctx->dc, ctx->gl);

	} else {
		r = wglMakeCurrent(NULL, NULL);
	}

	return r;
}
