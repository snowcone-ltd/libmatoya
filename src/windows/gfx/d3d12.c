// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod.h"
GFX_PROTOTYPES(_d3d12_)

#define COBJMACROS
#include <d3d12.h>

#include "gfx/viewport.h"
#include "gfx/fmt-dxgi.h"
#include "gfx/fmt.h"

static
#include "shaders/d3d11/ps.h"

static
#include "shaders/d3d11/vs.h"

#define D3D12_NUM_STAGING 3

#define D3D12_PITCH(w, bpp) \
	(((w) * (bpp) + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1))

struct d3d12_res {
	DXGI_FORMAT format;
	ID3D12Resource *buffer;
	ID3D12Resource *resource;
	D3D12_CPU_DESCRIPTOR_HANDLE dh;
	uint32_t width;
	uint32_t height;
};

struct d3d12 {
	MTY_ColorFormat format;
	struct gfx_uniforms ub;
	struct d3d12_res staging[D3D12_NUM_STAGING];

	ID3D12RootSignature *rs;
	ID3D12PipelineState *pipeline;
	ID3D12DescriptorHeap *srv_heap;
	ID3D12DescriptorHeap *sampler_heap;

	D3D12_GPU_DESCRIPTOR_HANDLE srv_dh;
	D3D12_GPU_DESCRIPTOR_HANDLE linear_dh;
	D3D12_GPU_DESCRIPTOR_HANDLE point_dh;

	ID3D12Resource *vb;
	ID3D12Resource *ib;
	ID3D12Resource *cb;
};

static const float D3D12_VERTEX_DATA[] = {
	-1.0f, -1.0f,  // position0 (bottom-left)
	 0.0f,  1.0f,  // texcoord0
	-1.0f,  1.0f,  // position1 (top-left)
	 0.0f,  0.0f,  // texcoord1
	 1.0f,  1.0f,  // position2 (top-right)
	 1.0f,  0.0f,  // texcoord2
	 1.0f, -1.0f,  // position3 (bottom-right)
	 1.0f,  1.0f   // texcoord3
};

static const DWORD D3D12_INDEX_DATA[] = {
	0, 1, 2,
	2, 3, 0
};

static HRESULT d3d12_buffer(ID3D12Device *device, const void *bdata, size_t size, ID3D12Resource **b)
{
	D3D12_HEAP_PROPERTIES hp = {0};
	hp.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC rd = {0};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rd.Width = size;
	rd.Height = 1;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_UNKNOWN;
	rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rd.SampleDesc.Count = 1;

	HRESULT e = ID3D12Device_CreateCommittedResource(device, &hp, D3D12_HEAP_FLAG_NONE, &rd,
		D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, b);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateComittedResource' failed with HRESULT 0x%X", e);
		goto except;
	}

	if (bdata) {
		void *data = NULL;
		e = ID3D12Resource_Map(*b, 0, NULL, &data);
		if (e != S_OK) {
			MTY_Log("'ID3D12Resource_Map' failed with HRESULT 0x%X", e);
			goto except;
		}

		memcpy(data, bdata, size);
		ID3D12Resource_Unmap(*b, 0, NULL);
	}

	except:

	if (e != S_OK && *b) {
		ID3D12Resource_Release(*b);
		*b = NULL;
	}

	return e;
}

static HRESULT d3d12_root_signature(ID3D12Device *device, ID3D12RootSignature **rs)
{
	D3D12_DESCRIPTOR_RANGE range[3] = {0};
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[0].NumDescriptors = D3D12_NUM_STAGING;
	range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	range[1].NumDescriptors = 1;
	range[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	range[2].NumDescriptors = 1;
	range[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rp[2] = {0};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].DescriptorTable.NumDescriptorRanges = 2;
	rp[0].DescriptorTable.pDescriptorRanges = range;
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[1].DescriptorTable.NumDescriptorRanges = 1;
	rp[1].DescriptorTable.pDescriptorRanges = &range[2];
	rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsdesc = {0};
	rsdesc.NumParameters = 2;
	rsdesc.pParameters = rp;
	rsdesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob *signature = NULL;
	HRESULT e = D3D12SerializeRootSignature(&rsdesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, NULL);
	if (e != S_OK) {
		MTY_Log("'D3D12SerializeRootSignature' failed with HRESULT 0x%X", e);
		return e;
	}

	e = ID3D12Device_CreateRootSignature(device, 0, signature->lpVtbl->GetBufferPointer(signature),
		signature->lpVtbl->GetBufferSize(signature), &IID_ID3D12RootSignature, rs);
	if (e != S_OK)
		MTY_Log("'ID3D12Device_CreateRootSignature' failed with HRESULT 0x%X", e);

	IUnknown_Release(signature);

	return e;
}

static HRESULT d3d12_pipeline(ID3D12Device *device, ID3D12RootSignature *rs, ID3D12PipelineState **pipeline)
{
	D3D12_INPUT_ELEMENT_DESC iedescs[2] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 2 * sizeof(float), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psdesc = {0};
	psdesc.InputLayout.pInputElementDescs = iedescs;
	psdesc.InputLayout.NumElements = 2;
	psdesc.pRootSignature = rs;
	psdesc.VS.pShaderBytecode = vs;
	psdesc.VS.BytecodeLength = sizeof(vs);
	psdesc.PS.pShaderBytecode = ps;
	psdesc.PS.BytecodeLength = sizeof(ps);
	psdesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psdesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psdesc.RasterizerState.DepthClipEnable = TRUE;
	psdesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	psdesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	psdesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	psdesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psdesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psdesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psdesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psdesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	psdesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psdesc.SampleMask = UINT_MAX;
	psdesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psdesc.NumRenderTargets = 1;
	psdesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	psdesc.SampleDesc.Count = 1;

	HRESULT e = ID3D12Device_CreateGraphicsPipelineState(device, &psdesc, &IID_ID3D12PipelineState, pipeline);
	if (e != S_OK)
		MTY_Log("'ID3D12Device_CreateGraphicsPipelineState' failed with HRESULT 0x%X", e);

	return e;
}

struct gfx *mty_d3d12_create(MTY_Device *device)
{
	struct d3d12 *ctx = MTY_Alloc(1, sizeof(struct d3d12));

	ID3D12Device *_device = (ID3D12Device *) device;

	// Pipeline
	HRESULT e = d3d12_root_signature(_device, &ctx->rs);
	if (e != S_OK)
		goto except;

	e = d3d12_pipeline(_device, ctx->rs, &ctx->pipeline);
	if (e != S_OK)
		goto except;

	// Shader resource views
	D3D12_DESCRIPTOR_HEAP_DESC dhdesc = {0};
	dhdesc.NumDescriptors = D3D12_NUM_STAGING + 1; // +1 for the constant buffer
	dhdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	dhdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	e = ID3D12Device_CreateDescriptorHeap(_device, &dhdesc, &IID_ID3D12DescriptorHeap, &ctx->srv_heap);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateDescriptorHeap' failed with HRESULT 0x%X", e);
		goto except;
	}

	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(ctx->srv_heap, &ctx->srv_dh);

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_dh = {0};
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(ctx->srv_heap, &cpu_dh);

	UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (uint8_t x = 0; x < D3D12_NUM_STAGING; x++) {
		ctx->staging[x].dh = cpu_dh;
		cpu_dh.ptr += inc;
	}

	// Constant buffer
	UINT cbsize = (sizeof(struct gfx_uniforms) + 255) & ~255; // MUST be 256-byte aligned

	e = d3d12_buffer(_device, NULL, cbsize, &ctx->cb);
	if (e != S_OK)
		goto except;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbdesc = {0};
	cbdesc.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(ctx->cb);
	cbdesc.SizeInBytes = cbsize;

	ID3D12Device_CreateConstantBufferView(_device, &cbdesc, cpu_dh);

	// Samplers
	memset(&dhdesc, 0, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	dhdesc.NumDescriptors = 2;
	dhdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	dhdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	e = ID3D12Device_CreateDescriptorHeap(_device, &dhdesc, &IID_ID3D12DescriptorHeap, &ctx->sampler_heap);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateDescriptorHeap' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE sampler_dh = {0};
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(ctx->sampler_heap, &sampler_dh);

	D3D12_GPU_DESCRIPTOR_HANDLE sampler_dh_gpu = {0};
	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(ctx->sampler_heap, &sampler_dh_gpu);

	UINT sampler_dsize = ID3D12Device_GetDescriptorHandleIncrementSize(_device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	// Sampler - linear
	D3D12_SAMPLER_DESC sd = {0};
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sd.MaxLOD = D3D12_FLOAT32_MAX;
	ID3D12Device_CreateSampler(_device, &sd, sampler_dh);

	ctx->linear_dh = sampler_dh_gpu;
	sampler_dh.ptr += sampler_dsize;
	sampler_dh_gpu.ptr += sampler_dsize;

	// Sampler - point
	sd.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	ID3D12Device_CreateSampler(_device, &sd, sampler_dh);

	ctx->point_dh = sampler_dh_gpu;

	// Vertex buffer
	e = d3d12_buffer(_device, D3D12_VERTEX_DATA, sizeof(D3D12_VERTEX_DATA), &ctx->vb);
	if (e != S_OK)
		goto except;

	// Index buffer
	e = d3d12_buffer(_device, D3D12_INDEX_DATA, sizeof(D3D12_INDEX_DATA), &ctx->ib);
	if (e != S_OK)
		goto except;

	except:

	if (e != S_OK)
		mty_d3d12_destroy((struct gfx **) &ctx, device);

	return (struct gfx *) ctx;
}

static void d3d12_destroy_resource(struct d3d12_res *res)
{
	if (res->buffer) {
		ID3D12Resource_Release(res->buffer);
		res->buffer = NULL;
	}

	if (res->resource) {
		ID3D12Resource_Release(res->resource);
		res->resource = NULL;
	}

	res->format = DXGI_FORMAT_UNKNOWN;
	res->width = 0;
	res->height = 0;
}

static bool d3d12_crop_copy(struct d3d12_res *res, MTY_Context *context, const uint8_t *image, uint32_t full_w,
	uint32_t w, uint32_t h, uint8_t bpp)
{
	ID3D12GraphicsCommandList *cl = (ID3D12GraphicsCommandList *) context;

	// Copy data to upload buffer
	uint8_t *data = NULL;
	HRESULT e = ID3D12Resource_Map(res->buffer, 0, NULL, &data);
	if (e != S_OK) {
		MTY_Log("'ID3D12Resource_Map' failed with HRESULT 0x%X", e);
		return false;
	}

	UINT pitch = D3D12_PITCH(w, bpp);

	for (uint32_t y = 0; y < h; y++)
		memcpy(data + y * pitch, image + y * full_w * bpp, w * bpp);

	ID3D12Resource_Unmap(res->buffer, 0, NULL);

	// Copy upload buffer to texture
	D3D12_RESOURCE_BARRIER rb = {0};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb.Transition.pResource = res->resource;
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &rb);

	D3D12_TEXTURE_COPY_LOCATION tsrc = {0};
	tsrc.pResource = res->buffer;
	tsrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	tsrc.PlacedFootprint.Footprint.Format = res->format;
	tsrc.PlacedFootprint.Footprint.Width = w;
	tsrc.PlacedFootprint.Footprint.Height = h;
	tsrc.PlacedFootprint.Footprint.Depth = 1;
	tsrc.PlacedFootprint.Footprint.RowPitch = pitch;

	D3D12_TEXTURE_COPY_LOCATION tdest = {0};
	tdest.pResource = res->resource;
	tdest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	ID3D12GraphicsCommandList_CopyTextureRegion(cl, &tdest, 0, 0, 0, &tsrc, NULL);

	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &rb);

	return true;
}

static bool d3d12_refresh_resource(struct gfx *gfx, MTY_Device *_device, MTY_Context *context, MTY_ColorFormat fmt,
	uint8_t plane, const uint8_t *image, uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp)
{
	struct d3d12 *ctx = (struct d3d12 *) gfx;

	bool r = true;

	struct d3d12_res *res = &ctx->staging[plane];
	DXGI_FORMAT format = FMT_PLANES[fmt][plane];

	// Resize
	if (!res->resource || w != res->width || h != res->height || format != res->format) {
		ID3D12Device *device = (ID3D12Device *) _device;

		d3d12_destroy_resource(res);

		// Upload buffer
		D3D12_HEAP_PROPERTIES hp = {0};
		hp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC desc = {0};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = D3D12_PITCH(w, bpp) * h;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.SampleDesc.Count = 1;

		HRESULT e = ID3D12Device_CreateCommittedResource(device, &hp, D3D12_HEAP_FLAG_NONE, &desc,
			 D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &res->buffer);
		if (e != S_OK) {
			MTY_Log("'ID3D12Device_CreateCommittedResource' failed with HRESULT 0x%X", e);
			r = false;
			goto except;
		}

		// Texture
		hp.Type = D3D12_HEAP_TYPE_DEFAULT;

		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = w;
		desc.Height = h;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = format;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.SampleDesc.Count = 1;

		e = ID3D12Device_CreateCommittedResource(device, &hp, D3D12_HEAP_FLAG_NONE, &desc,
			 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL, &IID_ID3D12Resource, &res->resource);
		if (e != S_OK) {
			MTY_Log("'ID3D12Device_CreateCommittedResource' failed with HRESULT 0x%X", e);
			r = false;
			goto except;
		}

		// Shader resourve view
		D3D12_SHADER_RESOURCE_VIEW_DESC sdesc = {0};
		sdesc.Format = format;
		sdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		sdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sdesc.Texture2D.MipLevels = 1;

		ID3D12Device_CreateShaderResourceView(device, res->resource, &sdesc, res->dh);

		res->width = w;
		res->height = h;
		res->format = format;
	}

	except:

	if (r) {
		// Upload
		r = d3d12_crop_copy(res, context, image, full_w, w, h, bpp);

	} else {
		d3d12_destroy_resource(res);
	}

	return r;
}

bool mty_d3d12_render(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest)
{
	struct d3d12 *ctx = (struct d3d12 *) gfx;

	ID3D12GraphicsCommandList *cl = (ID3D12GraphicsCommandList *) context;
	D3D12_CPU_DESCRIPTOR_HANDLE *_dest = (D3D12_CPU_DESCRIPTOR_HANDLE *) dest;

	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	// Refresh textures and load texture data
	// If format == MTY_COLOR_FORMAT_UNKNOWN, texture refreshing/loading is skipped and the previous frame is rendered
	if (!fmt_reload_textures(gfx, device, context, image, desc, d3d12_refresh_resource))
		return false;

	// Viewport
	D3D12_VIEWPORT vp = {0};
	mty_viewport(desc, &vp.TopLeftX, &vp.TopLeftY, &vp.Width, &vp.Height);

	ID3D12GraphicsCommandList_RSSetViewports(cl, 1, &vp);

	// Scissor
	D3D12_RECT scissor = {0};
	scissor.right = desc->viewWidth;
	scissor.bottom = desc->viewHeight;
	ID3D12GraphicsCommandList_RSSetScissorRects(cl, 1, &scissor);

	// Render target view
	if (_dest) {
		ID3D12GraphicsCommandList_OMSetRenderTargets(cl, 1, _dest, FALSE, NULL);

		const float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		ID3D12GraphicsCommandList_ClearRenderTargetView(cl, *_dest, color, 0, NULL);
	}

	// Set up pipeline
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(cl, ctx->rs);

	ID3D12DescriptorHeap *heaps[2] = {ctx->srv_heap, ctx->sampler_heap};
	ID3D12GraphicsCommandList_SetDescriptorHeaps(cl, 2, heaps);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(cl, 0, ctx->srv_dh);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(cl, 1,
		desc->filter == MTY_FILTER_NEAREST ? ctx->point_dh : ctx->linear_dh);

	ID3D12GraphicsCommandList_SetPipelineState(cl, ctx->pipeline);

	// Fill constant buffer
	struct gfx_uniforms cb = {
		.width = (float) desc->cropWidth,
		.height = (float) desc->cropHeight,
		.vp_height = (float) vp.Height,
		.effects[0] = desc->effects[0],
		.effects[1] = desc->effects[1],
		.levels[0] = desc->levels[0],
		.levels[1] = desc->levels[1],
		.planes = FMT_INFO[ctx->format].planes,
		.rotation = desc->rotation,
		.conversion = FMT_CONVERSION(ctx->format, desc->fullRangeYUV, desc->multiplyYUV),
	};

	if (memcmp(&ctx->ub, &cb, sizeof(struct gfx_uniforms))) {
		uint8_t *data = NULL;
		HRESULT e = ID3D12Resource_Map(ctx->cb, 0, NULL, &data);
		if (e != S_OK) {
			MTY_Log("'ID3D12Resource_Map' failed with HRESULT 0x%X", e);
			return false;
		}

		memcpy(data, &cb, sizeof(struct gfx_uniforms));

		ID3D12Resource_Unmap(ctx->cb, 0, NULL);

		ctx->ub = cb;
	}

	// Draw
	D3D12_VERTEX_BUFFER_VIEW vbview = {0};
	vbview.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(ctx->vb);
	vbview.StrideInBytes = 4 * sizeof(float);
	vbview.SizeInBytes = sizeof(D3D12_VERTEX_DATA);
	ID3D12GraphicsCommandList_IASetVertexBuffers(cl, 0, 1, &vbview);

	D3D12_INDEX_BUFFER_VIEW ibview = {0};
	ibview.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(ctx->ib);
	ibview.SizeInBytes = sizeof(D3D12_INDEX_DATA);
	ibview.Format = DXGI_FORMAT_R32_UINT;
	ID3D12GraphicsCommandList_IASetIndexBuffer(cl, &ibview);

	ID3D12GraphicsCommandList_IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12GraphicsCommandList_DrawIndexedInstanced(cl, 6, 1, 0, 0, 0);

	return true;
}

void mty_d3d12_destroy(struct gfx **gfx, MTY_Device *device)
{
	if (!gfx || !*gfx)
		return;

	struct d3d12 *ctx = (struct d3d12 *) *gfx;

	for (uint8_t x = 0; x < D3D12_NUM_STAGING; x++)
		d3d12_destroy_resource(&ctx->staging[x]);

	if (ctx->ib)
		ID3D12Resource_Release(ctx->ib);

	if (ctx->vb)
		ID3D12Resource_Release(ctx->vb);

	if (ctx->sampler_heap)
		ID3D12DescriptorHeap_Release(ctx->sampler_heap);

	if (ctx->cb)
		ID3D12Resource_Release(ctx->cb);

	if (ctx->srv_heap)
		ID3D12DescriptorHeap_Release(ctx->srv_heap);

	if (ctx->pipeline)
		ID3D12PipelineState_Release(ctx->pipeline);

	if (ctx->rs)
		ID3D12RootSignature_Release(ctx->rs);

	MTY_Free(ctx);
	*gfx = NULL;
}


// State

void *mty_d3d12_get_state(MTY_Device *device, MTY_Context *context)
{
	return NULL;
}

void mty_d3d12_set_state(MTY_Device *device, MTY_Context *context, void *state)
{
}

void mty_d3d12_free_state(void **state)
{
}
