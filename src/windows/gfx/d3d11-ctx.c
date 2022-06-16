// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_d3d11_)

#define COBJMACROS
#include <d3d11.h>
#include <dxgi1_6.h>

#define DXGI_FATAL(e) ( \
	(e) == DXGI_ERROR_DEVICE_REMOVED || \
	(e) == DXGI_ERROR_DEVICE_HUNG    || \
	(e) == DXGI_ERROR_DEVICE_RESET \
)

#define D3D11_SWFLAGS ( \
	DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | \
	DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING \
)

#define D3D11_CTX_WAIT 2000
#ifndef D3D11_CTX_DEBUG
	#define D3D11_CTX_DEBUG false
#endif

struct d3d11_ctx {
	HWND hwnd;
	uint32_t width;
	uint32_t height;
	MTY_Renderer *renderer;
	DXGI_FORMAT format;
	DXGI_FORMAT format_new;
	MTY_ColorSpace colorspace;
	MTY_ColorSpace colorspace_new;
	ID3D11Device *device;
	ID3D11DeviceContext *context;
	ID3D11Texture2D *back_buffer;
	IDXGISwapChain2 *swap_chain2;
	IDXGISwapChain3 *swap_chain3;
	IDXGISwapChain4 *swap_chain4;
	IDXGIFactory1 *factory1;
	HANDLE waitable;
	bool hdr_init;
	bool hdr_supported;
	bool hdr;
	bool composite_ui;
	MTY_HDRDesc hdr_desc;
	RECT window_bounds;
};

static void d3d11_ctx_get_size(struct d3d11_ctx *ctx, uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	GetClientRect(ctx->hwnd, &rect);

	*width = rect.right - rect.left;
	*height = rect.bottom - rect.top;
}

static bool d3d11_ctx_refresh_window_bounds(struct d3d11_ctx *ctx)
{
	bool changed = false;

	RECT window_bounds_new = {0};
	GetWindowRect(ctx->hwnd, &window_bounds_new);

	const LONG dt_left = window_bounds_new.left - ctx->window_bounds.left;
	const LONG dt_top = window_bounds_new.top - ctx->window_bounds.top;
	const LONG dt_right = window_bounds_new.right - ctx->window_bounds.right;
	const LONG dt_bottom = window_bounds_new.bottom - ctx->window_bounds.bottom;

	changed = dt_left || dt_top || dt_right || dt_bottom;

	ctx->window_bounds = window_bounds_new;

	return changed;
}

static bool d3d11_ctx_query_hdr_support(struct d3d11_ctx *ctx)
{
	bool r = false;

	// Courtesy of MSDN https://docs.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range

	// Iterate through the DXGI outputs associated with the DXGI adapter,
	// and find the output whose bounds have the greatest overlap with the
	// app window (i.e. the output for which the intersection area is the
	// greatest).

	// Must create the factory afresh each time, otherwise you'll get a stale value at the end
	if (ctx->factory1) {
		IDXGIFactory1_Release(ctx->factory1);
		ctx->factory1 = NULL;
	}
	HRESULT e = CreateDXGIFactory1(&IID_IDXGIFactory1, &ctx->factory1);
	if (e != S_OK) {
		MTY_Log("'CreateDXGIFactory1' failed with HRESULT 0x%X", e);
		return r;
	}

	// Get the retangle bounds of the app window
	const LONG ax1 = ctx->window_bounds.left;
	const LONG ay1 = ctx->window_bounds.top;
	const LONG ax2 = ctx->window_bounds.right;
	const LONG ay2 = ctx->window_bounds.bottom;

	// Go through the outputs of each and every adapter
	IDXGIOutput *current_output = NULL;
	IDXGIOutput *best_output = NULL;
	LONG best_intersect_area = -1;
	IDXGIAdapter1 *adapter1 = NULL;
	for (UINT j = 0; IDXGIFactory1_EnumAdapters1(ctx->factory1, j, &adapter1) != DXGI_ERROR_NOT_FOUND; j++) {
		for (UINT i = 0; IDXGIAdapter1_EnumOutputs(adapter1, i, &current_output) != DXGI_ERROR_NOT_FOUND; i++) {
			// Get the rectangle bounds of current output
			DXGI_OUTPUT_DESC desc = {0};
			e = IDXGIOutput_GetDesc(current_output, &desc);
			if (e != S_OK) {
				MTY_Log("'IDXGIOutput_GetDesc' failed with HRESULT 0x%X", e);
			} else {
				const RECT output_bounds = desc.DesktopCoordinates;
				const LONG bx1 = output_bounds.left;
				const LONG by1 = output_bounds.top;
				const LONG bx2 = output_bounds.right;
				const LONG by2 = output_bounds.bottom;

				// Compute the intersection and see if its the best fit
				// Courtesy of https://github.com/microsoft/DirectX-Graphics-Samples/blob/c79f839da1bb2db77d2306be5e4e664a5d23a36b/Samples/Desktop/D3D12HDR/src/D3D12HDR.cpp#L1046
				const LONG intersect_area = max(0, min(ax2, bx2) - max(ax1, bx1)) * max(0, min(ay2, by2) - max(ay1, by1));
				if (intersect_area > best_intersect_area) {
					if (best_output != NULL)
						IDXGIOutput_Release(best_output);

					best_output = current_output;
					best_intersect_area = intersect_area;

				} else {
					IDXGIOutput_Release(current_output);
				}
			}
		}

		IDXGIAdapter1_Release(adapter1);
	}

	// Having determined the output (display) upon which the app is primarily being
	// rendered, retrieve the HDR capabilities of that display by checking the color space.
	IDXGIOutput6 *output6 = NULL;
	e = IDXGIOutput_QueryInterface(best_output, &IID_IDXGIOutput6, &output6);
	if (e != S_OK) {
		MTY_Log("'IDXGIOutput_QueryInterface' failed with HRESULT 0x%X", e);
	} else {
		DXGI_OUTPUT_DESC1 desc1 = {0};
		e = IDXGIOutput6_GetDesc1(output6, &desc1);
		if (e != S_OK) {
			MTY_Log("'IDXGIOutput6_GetDesc1' failed with HRESULT 0x%X", e);

		} else {
			r = desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020; // this is the canonical check according to MSDN and NVIDIA
		}

		IDXGIOutput6_Release(output6);
	}

	IDXGIOutput_Release(best_output);

	return r;
}

static void d3d11_ctx_free_hdr(struct d3d11_ctx *ctx)
{
	if (ctx->swap_chain4)
		IDXGISwapChain4_Release(ctx->swap_chain4);

	if (ctx->swap_chain3)
		IDXGISwapChain3_Release(ctx->swap_chain3);

	ctx->swap_chain4 = NULL;
	ctx->swap_chain3 = NULL;
}

static void d3d11_ctx_free(struct d3d11_ctx *ctx)
{
	d3d11_ctx_free_hdr(ctx);

	if (ctx->back_buffer)
		ID3D11Texture2D_Release(ctx->back_buffer);

	if (ctx->waitable)
		CloseHandle(ctx->waitable);

	if (ctx->swap_chain2)
		IDXGISwapChain2_Release(ctx->swap_chain2);

	if (ctx->factory1)
		IDXGIFactory1_Release(ctx->factory1);

	if (ctx->context)
		ID3D11DeviceContext_Release(ctx->context);

	if (ctx->device)
		ID3D11Device_Release(ctx->device);

	ctx->back_buffer = NULL;
	ctx->waitable = NULL;
	ctx->swap_chain2 = NULL;
	ctx->factory1 = NULL;
	ctx->context = NULL;
	ctx->device = NULL;
}

static bool d3d11_ctx_init(struct d3d11_ctx *ctx)
{
	IDXGIDevice2 *device2 = NULL;
	IUnknown *unknown = NULL;
	IDXGIAdapter *adapter = NULL;
	IDXGIFactory2 *factory2 = NULL;
	IDXGISwapChain1 *swap_chain1 = NULL;

	ctx->format = DXGI_FORMAT_B8G8R8A8_UNORM;
	ctx->colorspace = MTY_COLOR_SPACE_SRGB;

	DXGI_SWAP_CHAIN_DESC1 sd = {0};
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.BufferCount = 2;
	sd.Flags = D3D11_SWFLAGS;

	D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	UINT flags = 0;
	if (D3D11_CTX_DEBUG)
		flags |= D3D11_CREATE_DEVICE_DEBUG;
	HRESULT e = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, levels,
		sizeof(levels) / sizeof(D3D_FEATURE_LEVEL), D3D11_SDK_VERSION, &ctx->device, NULL, &ctx->context);
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

	e = IDXGIFactory2_QueryInterface(factory2, &IID_IDXGIFactory1, &ctx->factory1);
	if (e != S_OK) {
		MTY_Log("'IDXGIFactory2_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
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

	// HDR init

	HRESULT e_hdr = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain3, &ctx->swap_chain3);
	if (e_hdr != S_OK) {
		MTY_Log("'IDXGISwapChain1_QueryInterface' failed with HRESULT 0x%X", e_hdr);
		goto except_hdr;
	}

	e_hdr = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain4, &ctx->swap_chain4);
	if (e_hdr != S_OK) {
		MTY_Log("'IDXGISwapChain1_QueryInterface' failed with HRESULT 0x%X", e_hdr);
		goto except_hdr;
	}

	ctx->hdr_init = true;
	d3d11_ctx_refresh_window_bounds(ctx);
	ctx->hdr_supported = d3d11_ctx_query_hdr_support(ctx);

	except_hdr:

	if (e_hdr != S_OK)
		d3d11_ctx_free_hdr(ctx);

	except:

	if (swap_chain1)
		IDXGISwapChain1_Release(swap_chain1);

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
		// DXGI_FORMAT_UNKNOWN will resize without changing the existing format
		HRESULT e = IDXGISwapChain2_ResizeBuffers(ctx->swap_chain2, 0, 0, 0,
			DXGI_FORMAT_UNKNOWN, D3D11_SWFLAGS);

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

	const bool hdr = ctx->colorspace_new != MTY_COLOR_SPACE_SRGB;

	if (ctx->hdr != hdr) {
		// If in HDR mode, we keep swap chain in HDR10 (rec2020 10-bit RGB + ST2084 PQ);
		// otherwise in SDR mode, it's the standard BGRA8 sRGB
		DXGI_FORMAT format = hdr ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
		DXGI_COLOR_SPACE_TYPE colorspace = hdr ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

		HRESULT e = IDXGISwapChain2_ResizeBuffers(ctx->swap_chain2, 0, 0, 0, format, D3D11_SWFLAGS);
		if (e == S_OK) {
			e = IDXGISwapChain3_SetColorSpace1(ctx->swap_chain3, colorspace);

			if (e == S_OK) {
				ctx->hdr = hdr;
				ctx->format = ctx->format_new;
				ctx->colorspace = ctx->colorspace_new;

			} else if (DXGI_FATAL(e)) {
				MTY_Log("'IDXGISwapChain3_SetColorSpace1' failed with HRESULT 0x%X", e);
				d3d11_ctx_free(ctx);
				d3d11_ctx_init(ctx);
			}

		} else if (DXGI_FATAL(e)) {
			MTY_Log("'IDXGISwapChain2_ResizeBuffers' failed with HRESULT 0x%X", e);
			d3d11_ctx_free(ctx);
			d3d11_ctx_init(ctx);
		}
	}

	if (ctx->hdr) {
		// Update to the latest known HDR metadata
		DXGI_HDR_METADATA_HDR10 hdr_desc = {0};
		hdr_desc.RedPrimary[0] = (UINT16) (ctx->hdr_desc.color_primary_red[0] * 50000); // primaries and white point are normalized to 50000
		hdr_desc.RedPrimary[1] = (UINT16) (ctx->hdr_desc.color_primary_red[1] * 50000);
		hdr_desc.GreenPrimary[0] = (UINT16) (ctx->hdr_desc.color_primary_green[0] * 50000);
		hdr_desc.GreenPrimary[1] = (UINT16) (ctx->hdr_desc.color_primary_green[1] * 50000);
		hdr_desc.BluePrimary[0] = (UINT16) (ctx->hdr_desc.color_primary_blue[0] * 50000);
		hdr_desc.BluePrimary[1] = (UINT16) (ctx->hdr_desc.color_primary_blue[1] * 50000);
		hdr_desc.WhitePoint[0] = (UINT16) (ctx->hdr_desc.white_point[0] * 50000);
		hdr_desc.WhitePoint[1] = (UINT16) (ctx->hdr_desc.white_point[1] * 50000);
		hdr_desc.MinMasteringLuminance = (UINT) ctx->hdr_desc.min_luminance * 10000; // MinMasteringLuminance is specified as 1/10000th of a nit
		hdr_desc.MaxMasteringLuminance = (UINT) ctx->hdr_desc.max_luminance;
		hdr_desc.MaxContentLightLevel = (UINT16) ctx->hdr_desc.max_content_light_level;
		hdr_desc.MaxFrameAverageLightLevel = (UINT16) ctx->hdr_desc.max_frame_average_light_level;

		HRESULT e = IDXGISwapChain4_SetHDRMetaData(ctx->swap_chain4, DXGI_HDR_METADATA_TYPE_HDR10, sizeof(hdr_desc), &hdr_desc);
		if (e != S_OK)
			MTY_Log("Unable to set HDR metadata: 'IDXGISwapChain4_SetHDRMetaData' failed with HRESULT 0x%X", e);
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

void mty_d3d11_ctx_present(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	if (ctx->back_buffer) {
		UINT flags = interval == 0 ? DXGI_PRESENT_ALLOW_TEARING : 0;

		HRESULT e = IDXGISwapChain2_Present(ctx->swap_chain2, interval, flags);

		ID3D11Texture2D_Release(ctx->back_buffer);
		ctx->back_buffer = NULL;

		ctx->composite_ui = false;

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

	ctx->colorspace_new = desc->colorspace;

	if (desc->hdrDescSpecified)
		ctx->hdr_desc = desc->hdrDesc;

	mty_d3d11_ctx_get_surface(gfx_ctx);

	if (ctx->back_buffer) {
		MTY_RenderDesc mutated = *desc;
		mutated.viewWidth = ctx->width;
		mutated.viewHeight = ctx->height;

		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
			(MTY_Context *) ctx->context, image, &mutated, (MTY_Surface *) ctx->back_buffer);

		ctx->composite_ui = ctx->hdr;
	}
}

void mty_d3d11_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	MTY_DrawData dd_mutated = *dd;
	dd_mutated.hdr = ctx->composite_ui;

	ctx->colorspace_new = dd_mutated.hdr ? MTY_COLOR_SPACE_HDR10 : MTY_COLOR_SPACE_SRGB;

	mty_d3d11_ctx_get_surface(gfx_ctx);

	if (ctx->back_buffer)
		MTY_RendererDrawUI(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
			(MTY_Context *) ctx->context, &dd_mutated, (MTY_Surface *) ctx->back_buffer);
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

bool mty_d3d11_ctx_make_current(struct gfx_ctx *gfx_ctx, bool current)
{
	return false;
}

bool mty_d3d11_ctx_hdr_supported(struct gfx_ctx *gfx_ctx)
{
	struct d3d11_ctx *ctx = (struct d3d11_ctx *) gfx_ctx;

	if (!ctx->hdr_init) {
		ctx->hdr_supported = false;

	} else {
		const bool adapter_reset = !ctx->factory1 || !IDXGIFactory1_IsCurrent(ctx->factory1);
		const bool window_moved = d3d11_ctx_refresh_window_bounds(ctx); // includes when moved to different display
		if (window_moved || adapter_reset) {
			ctx->hdr_supported = d3d11_ctx_query_hdr_support(ctx);
		}
	}

	return ctx->hdr_supported;
}
