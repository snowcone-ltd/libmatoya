// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_d3d12_)

#define COBJMACROS
#include <d3d12.h>
#include <dxgi1_4.h>

#define DXGI_FATAL(e) ( \
	(e) == DXGI_ERROR_DEVICE_REMOVED || \
	(e) == DXGI_ERROR_DEVICE_HUNG    || \
	(e) == DXGI_ERROR_DEVICE_RESET \
)

#define D3D12_SWFLAGS ( \
	DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | \
	DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING \
)

#define D3D12_CTX_BUFFERS 2
#define D3D12_CTX_WAIT    2000
#define D3D12_CTX_DEBUG   false

struct d3d12_ctx_buffer {
	ID3D12Resource *resource;
	D3D12_CPU_DESCRIPTOR_HANDLE dh;
};

struct d3d12_ctx {
	HWND hwnd;
	bool vsync;
	MTY_Renderer *renderer;
	uint32_t width;
	uint32_t height;

	struct d3d12_core {
		ID3D12Device *device;
		ID3D12CommandQueue *cq;
		ID3D12CommandAllocator *ca;
		ID3D12GraphicsCommandList *cl;
		ID3D12Fence *fence;
		ID3D12DescriptorHeap *rtv_heap;
		IDXGISwapChain3 *swap_chain3;
		struct d3d12_ctx_buffer *back_buffer;
		struct d3d12_ctx_buffer buffers[D3D12_CTX_BUFFERS];
		HANDLE waitable;
		UINT64 fval;
	} core;
};


// Core

static void d3d12_core_free_buffers(struct d3d12_core *core)
{
	for (uint8_t x = 0; x < D3D12_CTX_BUFFERS; x++)
		if (core->buffers[x].resource) {
			ID3D12Resource_Release(core->buffers[x].resource);
			core->buffers[x].resource = NULL;
		}
}

static void d3d12_core_free(struct d3d12_core *core)
{
	d3d12_core_free_buffers(core);

	if (core->waitable)
		CloseHandle(core->waitable);

	if (core->swap_chain3)
		IDXGISwapChain3_Release(core->swap_chain3);

	if (core->fence)
		ID3D12Fence_Release(core->fence);

	if (core->rtv_heap)
		ID3D12DescriptorHeap_Release(core->rtv_heap);

	if (core->cl)
		ID3D12GraphicsCommandList_Release(core->cl);

	if (core->ca)
		ID3D12CommandAllocator_Release(core->ca);

	if (core->cq)
		ID3D12CommandQueue_Release(core->cq);

	if (core->device)
		ID3D12Device_Release(core->device);

	memset(core, 0, sizeof(struct d3d12_core));
}

static HRESULT d3d12_core_refresh_buffers(struct d3d12_core *core)
{
	D3D12_CPU_DESCRIPTOR_HANDLE dh = {0};
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(core->rtv_heap, &dh);

	UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(core->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (uint8_t x = 0; x < D3D12_CTX_BUFFERS; x++) {
		HRESULT e = IDXGISwapChain3_GetBuffer(core->swap_chain3, x, &IID_ID3D12Resource, &core->buffers[x].resource);
		if (e != S_OK) {
			MTY_Log("'IDXGISwapChain3_GetBuffer' failed with HRESULT 0x%X", e);
			return e;
		}

		D3D12_RENDER_TARGET_VIEW_DESC desc = {0};
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		ID3D12Device_CreateRenderTargetView(core->device, core->buffers[x].resource, &desc, dh);
		core->buffers[x].dh = dh;

		dh.ptr += inc;
	}

	return S_OK;
}

static bool d3d12_core_init(HWND hwnd, struct d3d12_core *core)
{
	IUnknown *unknown = NULL;
	IDXGIFactory2 *factory2 = NULL;
	IDXGISwapChain1 *swap_chain1 = NULL;

	DXGI_SWAP_CHAIN_DESC1 sd = {0};
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.BufferCount = D3D12_CTX_BUFFERS;
	sd.Flags = D3D12_SWFLAGS;

	D3D12_COMMAND_QUEUE_DESC qdesc = {0};
	qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	D3D12_DESCRIPTOR_HEAP_DESC dhdesc = {0};
	dhdesc.NumDescriptors = D3D12_CTX_BUFFERS;
	dhdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	UINT fflags = 0;

	if (D3D12_CTX_DEBUG) {
		fflags = DXGI_CREATE_FACTORY_DEBUG;
		ID3D12Debug *debug = NULL;
		D3D12GetDebugInterface(&IID_ID3D12Debug, &debug);
		ID3D12Debug_EnableDebugLayer(debug);
	}

	HRESULT e = CreateDXGIFactory2(fflags, &IID_IDXGIFactory2, &factory2);
	if (e != S_OK) {
		MTY_Log("'CreateDXGIFactory2' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, &core->device);
	if (e != S_OK) {
		MTY_Log("'D3D12CreateDevice' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D12Device_CreateCommandQueue(core->device, &qdesc, &IID_ID3D12CommandQueue, &core->cq);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateCommandQueue' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D12Device_CreateCommandAllocator(core->device, D3D12_COMMAND_LIST_TYPE_DIRECT,
		&IID_ID3D12CommandAllocator, &core->ca);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateCommandAllocator' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D12Device_CreateCommandList(core->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, core->ca,
		NULL, &IID_ID3D12GraphicsCommandList, &core->cl);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateCommandList' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D12Device_CreateDescriptorHeap(core->device, &dhdesc, &IID_ID3D12DescriptorHeap, &core->rtv_heap);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateDescriptorHeap' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D12Device_CreateFence(core->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &core->fence);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateFence' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D12CommandQueue_QueryInterface(core->cq, &IID_IUnknown, &unknown);
	if (e != S_OK) {
		MTY_Log("'ID3D12CommandQueue_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIFactory2_CreateSwapChainForHwnd(factory2, unknown, hwnd, &sd, NULL, NULL, &swap_chain1);
	if (e != S_OK) {
		MTY_Log("'IDXGIFactory2_CreateSwapChainForHwnd' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain3, &core->swap_chain3);
	if (e != S_OK) {
		MTY_Log("'IDXGISwapChain1_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	core->waitable = IDXGISwapChain3_GetFrameLatencyWaitableObject(core->swap_chain3);
	if (!core->waitable) {
		e = !S_OK;
		MTY_Log("'IDXGISwapChain3_GetFrameLatencyWaitableObject' failed");
		goto except;
	}

	e = IDXGISwapChain3_SetMaximumFrameLatency(core->swap_chain3, 1);
	if (e != S_OK) {
		MTY_Log("'IDXGISwapChain3_SetMaximumFrameLatency' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDXGIFactory2_MakeWindowAssociation(factory2, hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
	if (e != S_OK) {
		MTY_Log("'IDXGIFactory2_MakeWindowAssociation' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = d3d12_core_refresh_buffers(core);
	if (e != S_OK)
		goto except;

	// The docs say that the app should wait on this handle even before the first frame
	DWORD we = WaitForSingleObjectEx(core->waitable, D3D12_CTX_WAIT, TRUE);
	if (we != WAIT_OBJECT_0)
		MTY_Log("'WaitForSingleObjectEx' failed with error 0x%X", we);

	except:

	if (swap_chain1)
		IDXGISwapChain1_Release(swap_chain1);

	if (unknown)
		IUnknown_Release(unknown);

	if (factory2)
		IDXGIFactory2_Release(factory2);

	if (e != S_OK)
		d3d12_core_free(core);

	return e == S_OK;
}


// Context

static void d3d12_ctx_get_size(struct d3d12_ctx *ctx, uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	GetClientRect(ctx->hwnd, &rect);

	*width = rect.right - rect.left;
	*height = rect.bottom - rect.top;
}

struct gfx_ctx *mty_d3d12_ctx_create(void *native_window, bool vsync)
{
	struct d3d12_ctx *ctx = MTY_Alloc(1, sizeof(struct d3d12_ctx));

	bool r = true;

	ctx->hwnd = (HWND) native_window;
	ctx->vsync = vsync;

	d3d12_ctx_get_size(ctx, &ctx->width, &ctx->height);

	r = d3d12_core_init(ctx->hwnd, &ctx->core);
	if (!r)
		goto except;

	ctx->renderer = MTY_RendererCreate();

	except:

	if (!r)
		mty_d3d12_ctx_destroy((struct gfx_ctx **) &ctx);

	return (struct gfx_ctx *) ctx;
}

void mty_d3d12_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct d3d12_ctx *ctx = (struct d3d12_ctx *) *gfx_ctx;

	MTY_RendererDestroy(&ctx->renderer);

	d3d12_core_free(&ctx->core);

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

MTY_Device *mty_d3d12_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	struct d3d12_ctx *ctx = (struct d3d12_ctx *) gfx_ctx;
	struct d3d12_core *core = &ctx->core;

	return (MTY_Device *) core->device;
}

MTY_Context *mty_d3d12_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct d3d12_ctx *ctx = (struct d3d12_ctx *) gfx_ctx;
	struct d3d12_core *core = &ctx->core;

	return (MTY_Context *) core->cl;
}

static HRESULT d3d12_ctx_refresh(struct d3d12_ctx *ctx)
{
	struct d3d12_core *core = &ctx->core;

	uint32_t width = ctx->width;
	uint32_t height = ctx->height;

	d3d12_ctx_get_size(ctx, &width, &height);

	if (ctx->width != width || ctx->height != height) {
		d3d12_core_free_buffers(core);

		HRESULT e = IDXGISwapChain3_ResizeBuffers(core->swap_chain3, 0, 0, 0,
			DXGI_FORMAT_UNKNOWN, D3D12_SWFLAGS);

		if (e == S_OK) {
			d3d12_core_refresh_buffers(core);

			ctx->width = width;
			ctx->height = height;
		}

		if (DXGI_FATAL(e)) {
			MTY_Log("'IDXGISwapChain3_ResizeBuffers' failed with HRESULT 0x%X", e);
			d3d12_core_free(core);
			d3d12_core_init(ctx->hwnd, core);
		}

		return e;
	}

	return S_OK;
}

MTY_Surface *mty_d3d12_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct d3d12_ctx *ctx = (struct d3d12_ctx *) gfx_ctx;
	struct d3d12_core *core = &ctx->core;

	if (!core->back_buffer) {
		HRESULT e = d3d12_ctx_refresh(ctx);
		if (e != S_OK)
			return NULL;

		UINT index = IDXGISwapChain3_GetCurrentBackBufferIndex(core->swap_chain3);
		core->back_buffer = &core->buffers[index];

		core->fval++;

		D3D12_RESOURCE_BARRIER rb = {0};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		rb.Transition.pResource = core->back_buffer->resource;
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		ID3D12GraphicsCommandList_ResourceBarrier(core->cl, 1, &rb);
	}

	return (MTY_Surface *) &core->back_buffer->dh;
}

void mty_d3d12_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct d3d12_ctx *ctx = (struct d3d12_ctx *) gfx_ctx;
	struct d3d12_core *core = &ctx->core;

	if (core->back_buffer) {
		D3D12_RESOURCE_BARRIER rb = {0};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		rb.Transition.pResource = core->back_buffer->resource;
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

		ID3D12GraphicsCommandList_ResourceBarrier(core->cl, 1, &rb);

		HRESULT e = ID3D12GraphicsCommandList_Close(core->cl);
		if (e != S_OK) {
			MTY_Log("'ID3D12GraphicsCommandList_Close' failed with HRESULT 0x%X", e);
			return;
		}

		ID3D12CommandList *cl = NULL;

		e = ID3D12GraphicsCommandList_QueryInterface(core->cl, &IID_ID3D12CommandList, &cl);
		if (e != S_OK) {
			MTY_Log("'ID3D12GraphicsCommandList_QueryInterface' failed with HRESULT 0x%X", e);
			return;
		}

		ID3D12CommandQueue_ExecuteCommandLists(core->cq, 1, &cl);
		ID3D12CommandList_Release(cl);

		UINT interval = ctx->vsync ? 1 : 0;
		UINT flags = ctx->vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING;

		e = IDXGISwapChain3_Present(core->swap_chain3, interval, flags);

		if (DXGI_FATAL(e)) {
			MTY_Log("'IDXGISwapChain3_Present' failed with HRESULT 0x%X", e);
			d3d12_core_free(core);
			d3d12_core_init(ctx->hwnd, core);

		} else {
			// Wait for completion
			e = ID3D12CommandQueue_Signal(core->cq, core->fence, core->fval);
			if (e != S_OK) {
				MTY_Log("'ID3D12CommandQueue_Signal' failed with HRESULT 0x%X", e);
				return;
			}

			if (ID3D12Fence_GetCompletedValue(core->fence) < core->fval) {
				e = ID3D12Fence_SetEventOnCompletion(core->fence, core->fval, NULL);
				if (e != S_OK) {
					MTY_Log("'ID3D12Fence_SetEventOnCompletion' failed with HRESULT 0x%X", e);
					return;
				}
			}

			// Clean up command list
			e = ID3D12CommandAllocator_Reset(core->ca);
			if (e != S_OK)
				MTY_Log("'ID3D12CommandAllocator_Reset' failed with HRESULT 0x%X", e);

			e = ID3D12GraphicsCommandList_Reset(core->cl, core->ca, NULL);
			if (e != S_OK)
				MTY_Log("'ID3D12GraphicsCommandList_Reset' failed with HRESULT 0x%X", e);

			// Wait for swap
			DWORD we = WaitForSingleObjectEx(core->waitable, D3D12_CTX_WAIT, TRUE);
			if (we != WAIT_OBJECT_0)
				MTY_Log("'WaitForSingleObjectEx' failed with error 0x%X", we);

			core->back_buffer = NULL;
		}
	}
}

void mty_d3d12_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct d3d12_ctx *ctx = (struct d3d12_ctx *) gfx_ctx;
	struct d3d12_core *core = &ctx->core;

	mty_d3d12_ctx_get_surface(gfx_ctx);

	if (core->back_buffer) {
		MTY_RenderDesc mutated = *desc;
		mutated.viewWidth = ctx->width;
		mutated.viewHeight = ctx->height;

		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_D3D12, (MTY_Device *) core->device,
			(MTY_Context *) core->cl, image, &mutated, (MTY_Surface *) &core->back_buffer->dh);
	}
}

void mty_d3d12_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct d3d12_ctx *ctx = (struct d3d12_ctx *) gfx_ctx;
	struct d3d12_core *core = &ctx->core;

	mty_d3d12_ctx_get_surface(gfx_ctx);

	if (core->back_buffer)
		MTY_RendererDrawUI(ctx->renderer, MTY_GFX_D3D12, (MTY_Device *) core->device,
			(MTY_Context *) core->cl, dd, (MTY_Surface *) &core->back_buffer->dh);
}

bool mty_d3d12_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct d3d12_ctx *ctx = (struct d3d12_ctx *) gfx_ctx;
	struct d3d12_core *core = &ctx->core;

	return MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_D3D12, (MTY_Device *) core->device,
		(MTY_Context *) core->cl, id, rgba, width, height);
}

bool mty_d3d12_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct d3d12_ctx *ctx = (struct d3d12_ctx *) gfx_ctx;

	return MTY_RendererHasUITexture(ctx->renderer, id);
}
