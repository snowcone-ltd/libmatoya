// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_d3d11_)

#define COBJMACROS
#include <d3d11.h>
#include <dxgi1_5.h>

#define DXGI_FATAL(e) ( \
	(e) == DXGI_ERROR_DEVICE_REMOVED || \
	(e) == DXGI_ERROR_DEVICE_HUNG    || \
	(e) == DXGI_ERROR_DEVICE_RESET \
)

#define D3D11_CTX_WAIT 2000

struct d3d11_ctx {
	HWND hwnd;
	bool vsync;
	uint32_t width;
	uint32_t height;
	MTY_Renderer *renderer;
	ID3D11Device *device;
	ID3D11DeviceContext *context;
	ID3D11Texture2D *back_buffer;
	IDXGISwapChain2 *swap_chain2;
	HANDLE waitable;
	UINT flags;
};

static void d3d11_ctx_get_size(struct d3d11_ctx *ctx, uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	GetClientRect(ctx->hwnd, &rect);

	*width = rect.right - rect.left;
	*height = rect.bottom - rect.top;
}

static void d3d11_ctx_free(struct d3d11_ctx *ctx)
{
	if (ctx->back_buffer)
		ID3D11Texture2D_Release(ctx->back_buffer);

	if (ctx->waitable)
		CloseHandle(ctx->waitable);

	if (ctx->swap_chain2)
		IDXGISwapChain2_Release(ctx->swap_chain2);

	if (ctx->context)
		ID3D11DeviceContext_Release(ctx->context);

	if (ctx->device)
		ID3D11Device_Release(ctx->device);

	ctx->back_buffer = NULL;
	ctx->waitable = NULL;
	ctx->swap_chain2 = NULL;
	ctx->context = NULL;
	ctx->device = NULL;
}

static bool d3d11_ctx_init(struct d3d11_ctx *ctx)
{
	IDXGIDevice2 *device2 = NULL;
	IUnknown *unknown = NULL;
	IDXGIAdapter *adapter = NULL;
	IDXGIFactory2 *factory2 = NULL;
	IDXGIFactory5 *factory5 = NULL;
	IDXGISwapChain1 *swap_chain1 = NULL;

	DXGI_SWAP_CHAIN_DESC1 sd = {0};
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.BufferCount = 2;

	ctx->flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
	sd.Flags = ctx->flags;

	HRESULT e = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL,
		0, D3D11_SDK_VERSION, &ctx->device, NULL, &ctx->context);
	if (e != S_OK) {
		MTY_Log("'D3D11CreateDevice' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Device_QueryInterface(ctx->device, &IID_IDXGIDevice2, &device2);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Device_QueryInterface(ctx->device, &IID_IUnknown, &unknown);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIDevice2_GetParent(device2, &IID_IDXGIAdapter, &adapter);
	if (e != S_OK) {
		MTY_Log("'IDXGIDevice2_GetParent' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, &factory2);
	if (e != S_OK) {
		MTY_Log("'IDXGIAdapter_GetParent' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Check for "tearing" (vsync off / gsync) support
	e = IDXGIFactory2_QueryInterface(factory2, &IID_IDXGIFactory5, &factory5);
	if (e == S_OK) {
		BOOL support = FALSE;
		e = IDXGIFactory5_CheckFeatureSupport(factory5, DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&support, sizeof(BOOL));

		if (e == S_OK && support) {
			ctx->flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
			sd.Flags = ctx->flags;
		}
	}

	e = IDXGIFactory2_CreateSwapChainForHwnd(factory2, unknown, ctx->hwnd, &sd, NULL, NULL, &swap_chain1);
	if (e != S_OK) {
		MTY_Log("'IDXGIFactory2_CreateSwapChainForHwnd' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain2, &ctx->swap_chain2);
	if (e != S_OK) {
		MTY_Log("'IDXGISwapChain1_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	ctx->waitable = IDXGISwapChain2_GetFrameLatencyWaitableObject(ctx->swap_chain2);
	if (!ctx->waitable) {
		e = !S_OK;
		MTY_Log("'IDXGISwapChain2_GetFrameLatencyWaitableObject' failed");
		goto except;
	}

	e = IDXGISwapChain2_SetMaximumFrameLatency(ctx->swap_chain2, 1);
	if (e != S_OK) {
		MTY_Log("'IDXGISwapChain2_SetMaximumFrameLatency' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIFactory2_MakeWindowAssociation(factory2, ctx->hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
	if (e != S_OK) {
		MTY_Log("'IDXGIFactory2_MakeWindowAssociation' failed with HRESULT 0x%X", e);
		goto except;
	}

	// The docs say that the app should wait on this handle even before the first frame
	DWORD we = WaitForSingleObjectEx(ctx->waitable, D3D11_CTX_WAIT, TRUE);
	if (we != WAIT_OBJECT_0)
		MTY_Log("'WaitForSingleObjectEx' failed with error 0x%X", we);

	except:

	if (swap_chain1)
		IDXGISwapChain1_Release(swap_chain1);

	if (factory5)
		IDXGIFactory5_Release(factory5);

	if (factory2)
		IDXGIFactory2_Release(factory2);

	if (adapter)
		IDXGIAdapter_Release(adapter);

	if (unknown)
		IUnknown_Release(unknown);

	if (device2)
		IDXGIDevice2_Release(device2);

	if (e != S_OK)
		d3d11_ctx_free(ctx);

	return e == S_OK;
}

struct gfx_ctx *mty_d3d11_ctx_create(void *native_window, bool vsync)
{
	struct d3d11_ctx *ctx = MTY_Alloc(1, sizeof(struct d3d11_ctx));
	ctx->hwnd = (HWND) native_window;
	ctx->vsync = vsync;
	ctx->renderer = MTY_RendererCreate();

	d3d11_ctx_get_size(ctx, &ctx->width, &ctx->height);

	if (!d3d11_ctx_init(ctx))
		mty_d3d11_ctx_destroy((struct gfx_ctx **) &ctx);

	return (struct gfx_ctx *) ctx;
}

void mty_d3d11_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct d3d11_ctx *ctx = (struct d3d11_ctx *) *gfx_ctx;

	MTY_RendererDestroy(&ctx->renderer);
	d3d11_ctx_free(ctx);

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

MTY_Device *mty_d3d11_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	return (MTY_Device *) ctx->device;
}

MTY_Context *mty_d3d11_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	return (MTY_Context *) ctx->context;
}

static void d3d11_ctx_refresh(struct d3d11_ctx *ctx)
{
	uint32_t width = ctx->width;
	uint32_t height = ctx->height;

	d3d11_ctx_get_size(ctx, &width, &height);

	if (ctx->width != width || ctx->height != height) {
		HRESULT e = IDXGISwapChain2_ResizeBuffers(ctx->swap_chain2, 0, 0, 0,
			DXGI_FORMAT_UNKNOWN, ctx->flags);

		if (e == S_OK) {
			ctx->width = width;
			ctx->height = height;
		}

		if (DXGI_FATAL(e)) {
			MTY_Log("'IDXGISwapChain2_ResizeBuffers' failed with HRESULT 0x%X", e);
			d3d11_ctx_free(ctx);
			d3d11_ctx_init(ctx);
		}
	}
}

MTY_Surface *mty_d3d11_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	if (!ctx->swap_chain2)
		return (MTY_Surface *) ctx->back_buffer;

	if (!ctx->back_buffer) {
		d3d11_ctx_refresh(ctx);

		HRESULT e = IDXGISwapChain2_GetBuffer(ctx->swap_chain2, 0, &IID_ID3D11Texture2D, &ctx->back_buffer);
		if (e != S_OK)
			MTY_Log("'IDXGISwapChain2_GetBuffer' failed with HRESULT 0x%X", e);
	}

	return (MTY_Surface *) ctx->back_buffer;
}

void mty_d3d11_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	if (ctx->back_buffer) {
		bool tearing = ctx->vsync && (ctx->flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);

		UINT interval = tearing ? 0 : 1;
		UINT flags = tearing ? DXGI_PRESENT_ALLOW_TEARING : 0;

		HRESULT e = IDXGISwapChain2_Present(ctx->swap_chain2, interval, flags);

		ID3D11Texture2D_Release(ctx->back_buffer);
		ctx->back_buffer = NULL;

		if (DXGI_FATAL(e)) {
			MTY_Log("'IDXGISwapChain2_Present' failed with HRESULT 0x%X", e);
			d3d11_ctx_free(ctx);
			d3d11_ctx_init(ctx);

		} else {
			DWORD we = WaitForSingleObjectEx(ctx->waitable, D3D11_CTX_WAIT, TRUE);
			if (we != WAIT_OBJECT_0)
				MTY_Log("'WaitForSingleObjectEx' failed with error 0x%X", we);
		}
	}
}

void mty_d3d11_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	mty_d3d11_ctx_get_surface(gfx_ctx);

	if (ctx->back_buffer) {
		MTY_RenderDesc mutated = *desc;
		mutated.viewWidth = ctx->width;
		mutated.viewHeight = ctx->height;

		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
			(MTY_Context *) ctx->context, image, &mutated, (MTY_Surface *) ctx->back_buffer);
	}
}

void mty_d3d11_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	mty_d3d11_ctx_get_surface(gfx_ctx);

	if (ctx->back_buffer)
		MTY_RendererDrawUI(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
			(MTY_Context *) ctx->context, dd, (MTY_Surface *) ctx->back_buffer);
}

bool mty_d3d11_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	return MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
		(MTY_Context *) ctx->context, id, rgba, width, height);
}

bool mty_d3d11_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	return MTY_RendererHasUITexture(ctx->renderer, id);
}
