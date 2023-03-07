// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_d3d9_)

#define COBJMACROS
#include <d3d9.h>

#define D3D_FATAL(e) ( \
	(e) == D3DERR_DEVICEREMOVED  || \
	(e) == D3DERR_DEVICELOST     || \
	(e) == D3DERR_DEVICEHUNG     || \
	(e) == D3DERR_DEVICENOTRESET \
)

struct d3d9_ctx {
	HWND hwnd;
	bool vsync;
	uint32_t width;
	uint32_t height;
	IDirect3D9Ex *factory;
	IDirect3DDevice9Ex *device;
	IDirect3DDevice9 *device_og;
	IDirect3DSwapChain9Ex *swap_chain;
	IDirect3DSurface9 *back_buffer;
};

static void d3d9_ctx_get_size(struct d3d9_ctx *ctx, uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	GetClientRect(ctx->hwnd, &rect);

	*width = rect.right - rect.left;
	*height = rect.bottom - rect.top;
}

static void d3d9_ctx_free(struct d3d9_ctx *ctx)
{
	if (ctx->back_buffer)
		IDirect3DSurface9_Release(ctx->back_buffer);

	if (ctx->swap_chain)
		IDirect3DSwapChain9Ex_Release(ctx->swap_chain);

	if (ctx->device_og)
		IDirect3DDevice9_Release(ctx->device_og);

	if (ctx->device)
		IDirect3DDevice9Ex_Release(ctx->device);

	if (ctx->factory)
		IDirect3D9Ex_Release(ctx->factory);

	ctx->back_buffer = NULL;
	ctx->swap_chain = NULL;
	ctx->device_og = NULL;
	ctx->device = NULL;
	ctx->factory = NULL;
}

static bool d3d9_ctx_init(struct d3d9_ctx *ctx)
{
	IDirect3DSwapChain9 *swap_chain = NULL;

	HRESULT e = Direct3DCreate9Ex(D3D_SDK_VERSION, &ctx->factory);
	if (e != S_OK) {
		MTY_Log("'Direct3DCreate9Ex' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3DPRESENT_PARAMETERS pp = {0};
	pp.BackBufferFormat = D3DFMT_UNKNOWN;
	pp.BackBufferCount = 1;
	pp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
	pp.hDeviceWindow = ctx->hwnd;
	pp.Windowed = TRUE;
	pp.PresentationInterval = ctx->vsync ?
		D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

	DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE |
		D3DCREATE_NOWINDOWCHANGES | D3DCREATE_MULTITHREADED | D3DCREATE_DISABLE_PSGP_THREADING;

	e = IDirect3D9Ex_CreateDeviceEx(ctx->factory, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, ctx->hwnd,
		flags, &pp, NULL, &ctx->device);
	if (e != S_OK) {
		MTY_Log("'IDirect3D9Ex_CreateDeviceEx' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDirect3DDevice9Ex_SetMaximumFrameLatency(ctx->device, 1);
	if (e != S_OK) {
		MTY_Log("'IDirect3DDevice9Ex_SetMaximumFrameLatency' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDirect3DDevice9Ex_QueryInterface(ctx->device, &IID_IDirect3DDevice9, &ctx->device_og);
	if (e != S_OK) {
		MTY_Log("'IDirect3DDevice9Ex_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDirect3DDevice9Ex_GetSwapChain(ctx->device, 0, &swap_chain);
	if (e != S_OK) {
		MTY_Log("'IDirect3DDevice9Ex_GetSwapChain' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDirect3DSwapChain9_QueryInterface(swap_chain, &IID_IDirect3DSwapChain9Ex, &ctx->swap_chain);
	if (e != S_OK) {
		MTY_Log("'IDirect3DSwapChain9_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	except:

	if (swap_chain)
		IDirect3DSwapChain9_Release(swap_chain);

	if (e != S_OK)
		d3d9_ctx_free(ctx);

	return e == S_OK;
}

struct gfx_ctx *mty_d3d9_ctx_create(void *native_window, bool vsync)
{
	struct d3d9_ctx *ctx = MTY_Alloc(1, sizeof(struct d3d9_ctx));
	ctx->hwnd = (HWND) native_window;
	ctx->vsync = vsync;

	d3d9_ctx_get_size(ctx, &ctx->width, &ctx->height);

	if (!d3d9_ctx_init(ctx))
		mty_d3d9_ctx_destroy((struct gfx_ctx **) &ctx);

	return (struct gfx_ctx *) ctx;
}

void mty_d3d9_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct d3d9_ctx *ctx = (struct d3d9_ctx *) *gfx_ctx;

	d3d9_ctx_free(ctx);

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

void mty_d3d9_ctx_get_size(struct gfx_ctx *gfx_ctx, uint32_t *w, uint32_t *h)
{
	struct d3d9_ctx *ctx = (struct d3d9_ctx *) gfx_ctx;

	*w = ctx->width;
	*h = ctx->height;
}

MTY_Device *mty_d3d9_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	struct d3d9_ctx *ctx = (struct d3d9_ctx *) gfx_ctx;

	return (MTY_Device *) ctx->device_og;
}

MTY_Context *mty_d3d9_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	return NULL;
}

static void d3d9_ctx_refresh(struct d3d9_ctx *ctx)
{
	// ResetEx will fail if the window is minimized
	if (IsIconic(ctx->hwnd))
		return;

	uint32_t width = ctx->width;
	uint32_t height = ctx->height;

	d3d9_ctx_get_size(ctx, &width, &height);

	if (ctx->width != width || ctx->height != height) {
		D3DPRESENT_PARAMETERS pp = {0};
		pp.BackBufferFormat = D3DFMT_UNKNOWN;
		pp.BackBufferCount = 1;
		pp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
		pp.hDeviceWindow = ctx->hwnd;
		pp.Windowed = TRUE;
		pp.PresentationInterval = ctx->vsync ?
			D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

		HRESULT e = IDirect3DDevice9Ex_ResetEx(ctx->device, &pp, NULL);

		if (e == S_OK) {
			ctx->width = width;
			ctx->height = height;

		} else {
			MTY_Log("'IDirect3DDevice9Ex_ResetEx' failed with HRESULT 0x%X", e);
		}

		if (D3D_FATAL(e)) {
			d3d9_ctx_free(ctx);
			d3d9_ctx_init(ctx);
		}
	}
}

MTY_Surface *mty_d3d9_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct d3d9_ctx *ctx = (struct d3d9_ctx *) gfx_ctx;

	if (!ctx->device)
		return NULL;

	if (!ctx->back_buffer) {
		d3d9_ctx_refresh(ctx);

		HRESULT e = IDirect3DSwapChain9Ex_GetBackBuffer(ctx->swap_chain, 0,
			D3DBACKBUFFER_TYPE_MONO, &ctx->back_buffer);
		if (e != S_OK) {
			MTY_Log("'IDirect3DSwapChain9Ex_GetBackBuffer' failed with HRESULT 0x%X", e);
			goto except;
		}

		e = IDirect3DDevice9Ex_BeginScene(ctx->device);
		if (e != S_OK) {
			MTY_Log("'IDirect3DDevice9Ex_BeginScene' failed with HRESULT 0x%X", e);
			goto except;
		}

		except:

		if (e != S_OK) {
			if (ctx->back_buffer) {
				IDirect3DSurface9_Release(ctx->back_buffer);
				ctx->back_buffer = NULL;
			}
		}
	}

	return (MTY_Surface *) ctx->back_buffer;
}

void mty_d3d9_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct d3d9_ctx *ctx = (struct d3d9_ctx *) gfx_ctx;

	if (ctx->back_buffer) {
		HRESULT e = IDirect3DDevice9Ex_EndScene(ctx->device);

		if (e == S_OK) {
			e = IDirect3DDevice9Ex_PresentEx(ctx->device, NULL, NULL, NULL, NULL, 0);

			if (D3D_FATAL(e)) {
				MTY_Log("'IDirect3DDevice9Ex_PresentEx' failed with HRESULT 0x%X", e);
				d3d9_ctx_free(ctx);
				d3d9_ctx_init(ctx);
			}

		} else {
			MTY_Log("'IDirect3DDevice9Ex_EndScene' failed with HRESULT 0x%X", e);
		}

		if (ctx->back_buffer) {
			IDirect3DSurface9_Release(ctx->back_buffer);
			ctx->back_buffer = NULL;
		}
	}
}

bool mty_d3d9_ctx_lock(struct gfx_ctx *gfx_ctx)
{
	return true;
}

void mty_d3d9_ctx_unlock(void)
{
}
