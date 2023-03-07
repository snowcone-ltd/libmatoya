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
	uint32_t fb0;
};

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	struct gl_ctx *ctx = MTY_Alloc(1, sizeof(struct gl_ctx));
	ctx->hwnd = (HWND) native_window;

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

	BOOL (WINAPI *wglSwapIntervalEXT)(int inverval) = (void *) wglGetProcAddress("wglSwapIntervalEXT");
	if (!wglSwapIntervalEXT)
		MTY_Log("'wglGetProcAddress' failed to find 'wglSwapIntervalEXT'");

	if (wglSwapIntervalEXT)
		wglSwapIntervalEXT(vsync ? 1 : 0);

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

	wglMakeCurrent(NULL, NULL);

	if (ctx->gl)
		wglDeleteContext(ctx->gl);

	if (ctx->hwnd && ctx->dc)
		ReleaseDC(ctx->hwnd, ctx->dc);

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

void mty_gl_ctx_get_size(struct gfx_ctx *gfx_ctx, uint32_t *w, uint32_t *h)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	RECT rect = {0};
	GetClientRect(ctx->hwnd, &rect);

	*w = rect.right - rect.left;
	*h = rect.bottom - rect.top;
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

	if (!wglSwapLayerBuffers(ctx->dc, WGL_SWAP_MAIN_PLANE))
		MTY_Log("'wglSwapLayerBuffers' failed with error 0x%X", GetLastError());

	glFinish();
}

bool mty_gl_ctx_lock(struct gfx_ctx *gfx_ctx)
{
	return true;
}

void mty_gl_ctx_unlock(void)
{
}
