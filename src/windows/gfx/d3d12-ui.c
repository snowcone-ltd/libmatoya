// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ui.h"
GFX_UI_PROTOTYPES(_d3d12_)

#include <stdio.h>
#include <math.h>

#define COBJMACROS
#include <d3d12.h>

static
#include "shaders/d3d11/psui.h"

static
#include "shaders/d3d11/vsui.h"

#define D3D12_UI_PITCH(w, bpp) \
	(((w) * (bpp) + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1))

struct d3d12_ui_texture {
	ID3D12Resource *buffer;
	ID3D12Resource *resource;
	ID3D12DescriptorHeap *heap;
	D3D12_GPU_DESCRIPTOR_HANDLE dh;
	uint32_t w;
	uint32_t h;
	bool transfer;
};

struct d3d12_ui_buffer {
	ID3D12Resource *res;
	uint32_t len;
};

struct d3d12_ui {
	ID3D12RootSignature *rs;
	ID3D12PipelineState *pipeline;
	struct d3d12_ui_buffer vb;
	struct d3d12_ui_buffer ib;
	struct d3d12_ui_texture *clear_tex;
};

struct d3d12_ui_cb {
	float proj[4][4];
};

struct gfx_ui *mty_d3d12_ui_create(MTY_Device *device)
{
	struct d3d12_ui *ctx = MTY_Alloc(1, sizeof(struct d3d12_ui));

	ID3D12Device *_device = (ID3D12Device *) device;

	// Root signature
	D3D12_ROOT_PARAMETER param[2] = {0};
	param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	param[0].Constants.Num32BitValues = 16;
	param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_DESCRIPTOR_RANGE descRange = {0};
	descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descRange.NumDescriptors = 1;

	param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param[1].DescriptorTable.NumDescriptorRanges = 1;
	param[1].DescriptorTable.pDescriptorRanges = &descRange;
	param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC staticSampler = {0};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC desc = {0};
	desc.NumParameters = 2;
	desc.pParameters = param;
	desc.NumStaticSamplers = 1;
	desc.pStaticSamplers = &staticSampler;
	desc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	ID3DBlob *blob = NULL;
	HRESULT e = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, NULL);
	if (e != S_OK) {
		MTY_Log("'D3D12SerializeRootSignature' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D12Device_CreateRootSignature(_device, 0, blob->lpVtbl->GetBufferPointer(blob),
		blob->lpVtbl->GetBufferSize(blob), &IID_ID3D12RootSignature, &ctx->rs);

	IUnknown_Release(blob);

	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateRootSignature' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Pipeline
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {0};
	psoDesc.VS.pShaderBytecode = vsui;
	psoDesc.VS.BytecodeLength = sizeof(vsui);
	psoDesc.PS.pShaderBytecode = psui;
	psoDesc.PS.BytecodeLength = sizeof(psui);
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.pRootSignature = ctx->rs;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	D3D12_INPUT_ELEMENT_DESC local_layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(MTY_Vtx, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(MTY_Vtx, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(MTY_Vtx, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	psoDesc.InputLayout.pInputElementDescs = local_layout;
	psoDesc.InputLayout.NumElements = 3;

	D3D12_BLEND_DESC *bdesc = &psoDesc.BlendState;
	bdesc->RenderTarget[0].BlendEnable = TRUE;
	bdesc->RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	bdesc->RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	bdesc->RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	bdesc->RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	bdesc->RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	bdesc->RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	bdesc->RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC *rdesc = &psoDesc.RasterizerState;
	rdesc->FillMode = D3D12_FILL_MODE_SOLID;
	rdesc->CullMode = D3D12_CULL_MODE_NONE;
	rdesc->DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rdesc->DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rdesc->SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rdesc->DepthClipEnable = TRUE;
	rdesc->ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_DEPTH_STENCIL_DESC *sdesc = &psoDesc.DepthStencilState;
	sdesc->DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	sdesc->DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sdesc->FrontFace.StencilFailOp = sdesc->FrontFace.StencilDepthFailOp =
		sdesc->FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	sdesc->FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sdesc->BackFace = sdesc->FrontFace;

	e = ID3D12Device_CreateGraphicsPipelineState(_device, &psoDesc, &IID_ID3D12PipelineState, &ctx->pipeline);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateGraphicsPipelineState' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Dummy clear texture
	void *rgba = MTY_Alloc(256 * 256, 4);
	ctx->clear_tex = mty_d3d12_ui_create_texture((struct gfx_ui *) ctx, device, rgba, 256, 256);
	MTY_Free(rgba);

	if (!ctx->clear_tex) {
		e = !S_OK;
		goto except;
	}

	except:

	if (e != S_OK)
		mty_d3d12_ui_destroy((struct gfx_ui **) &ctx, device);

	return (struct gfx_ui *) ctx;
}

static HRESULT d3d12_ui_resize_buffer(ID3D12Device *device, struct d3d12_ui_buffer *buf,
	uint32_t len, uint32_t incr, uint32_t element_size)
{
	if (buf->len < len) {
		if (buf->res) {
			ID3D12Resource_Release(buf->res);
			buf->res = NULL;
		}

		D3D12_HEAP_PROPERTIES hp = {0};
		hp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC rd = {0};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = (len + incr) * element_size;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rd.SampleDesc.Count = 1;

		HRESULT e = ID3D12Device_CreateCommittedResource(device, &hp, D3D12_HEAP_FLAG_NONE, &rd,
			D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &buf->res);

		if (e != S_OK) {
			MTY_Log("'ID3D12Device_CreateComittedResource' failed with HRESULT 0x%X", e);
			return e;
		}

		buf->len = len + incr;
	}

	return S_OK;
}

static void d3d12_ui_set_srv(struct d3d12_ui *ctx, ID3D12Device *device, ID3D12GraphicsCommandList *cl,
	MTY_Hash *cache, uint32_t texture, void **prev_tex)
{
	struct d3d12_ui_texture *tex = MTY_HashGetInt(cache, texture);

	if (!tex)
		tex = ctx->clear_tex;

	if (*prev_tex == tex)
		return;

	if (!tex->transfer) {
		D3D12_RESOURCE_BARRIER rb = {0};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		rb.Transition.pResource = tex->resource;
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &rb);

		D3D12_TEXTURE_COPY_LOCATION tsrc = {0};
		tsrc.pResource = tex->buffer;
		tsrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		tsrc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tsrc.PlacedFootprint.Footprint.Width = tex->w;
		tsrc.PlacedFootprint.Footprint.Height = tex->h;
		tsrc.PlacedFootprint.Footprint.Depth = 1;
		tsrc.PlacedFootprint.Footprint.RowPitch = D3D12_UI_PITCH(tex->w, 4);

		D3D12_TEXTURE_COPY_LOCATION tdest = {0};
		tdest.pResource = tex->resource;
		tdest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		ID3D12GraphicsCommandList_CopyTextureRegion(cl, &tdest, 0, 0, 0, &tsrc, NULL);

		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &rb);

		tex->transfer = true;
	}

	ID3D12GraphicsCommandList_SetDescriptorHeaps(cl, 1, &tex->heap);
	ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(cl, 1, tex->dh);

	*prev_tex = tex;
}

bool mty_d3d12_ui_render(struct gfx_ui *gfx_ui, MTY_Device *device, MTY_Context *context,
	const MTY_DrawData *dd, MTY_Hash *cache, MTY_Surface *dest)
{
	struct d3d12_ui *ctx = (struct d3d12_ui *) gfx_ui;

	ID3D12Device *_device = (ID3D12Device *) device;
	ID3D12GraphicsCommandList *cl = (ID3D12GraphicsCommandList *) context;
	D3D12_CPU_DESCRIPTOR_HANDLE *_dest = (D3D12_CPU_DESCRIPTOR_HANDLE *) dest;

	// Prevent rendering under invalid scenarios
	if (dd->displaySize.x <= 0 || dd->displaySize.y <= 0 || dd->cmdListLength == 0)
		return false;

	// Resize vertex and index buffers if necessary
	HRESULT e = d3d12_ui_resize_buffer(_device, &ctx->vb, dd->vtxTotalLength, GFX_UI_VTX_INCR, sizeof(MTY_Vtx));
	if (e != S_OK)
		return false;

	e = d3d12_ui_resize_buffer(_device, &ctx->ib, dd->idxTotalLength, GFX_UI_IDX_INCR, sizeof(uint16_t));
	if (e != S_OK)
		return false;

	// Map both vertex and index buffers and bulk copy the data
	MTY_Vtx *vtx_dst = NULL;
	uint16_t *idx_dst = NULL;

	e = ID3D12Resource_Map(ctx->vb.res, 0, NULL, &vtx_dst);
	if (e != S_OK) {
		MTY_Log("'ID3D12Resource_Map' failed with HRESULT 0x%X", e);
		return false;
	}

	e = ID3D12Resource_Map(ctx->ib.res, 0, NULL, &idx_dst);
	if (e != S_OK) {
		MTY_Log("'ID3D12Resource_Map' failed with HRESULT 0x%X", e);
		return false;
	}

	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		MTY_CmdList *cmdList = &dd->cmdList[x];

		memcpy(vtx_dst, cmdList->vtx, cmdList->vtxLength * sizeof(MTY_Vtx));
		memcpy(idx_dst, cmdList->idx, cmdList->idxLength * sizeof(uint16_t));

		vtx_dst += cmdList->vtxLength;
		idx_dst += cmdList->idxLength;
	}

	ID3D12Resource_Unmap(ctx->ib.res, 0, NULL);
	ID3D12Resource_Unmap(ctx->vb.res, 0, NULL);

	// Set viewport based on display size
	D3D12_VIEWPORT vp = {0};
	vp.Width = dd->displaySize.x;
	vp.Height = dd->displaySize.y;
	vp.MaxDepth = 1.0f;
	ID3D12GraphicsCommandList_RSSetViewports(cl, 1, &vp);

	// Set render target (wraps the texture)
	if (_dest) {
		ID3D12GraphicsCommandList_OMSetRenderTargets(cl, 1, _dest, FALSE, NULL);

		// Clear render target to black
		if (dd->clear) {
			const float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
			ID3D12GraphicsCommandList_ClearRenderTargetView(cl, *_dest, color, 0, NULL);
		}
	}

	// Update the vertex shader's projection data based on the current display size
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(cl, ctx->rs);
	ID3D12GraphicsCommandList_SetPipelineState(cl, ctx->pipeline);

	float L = 0;
	float R = dd->displaySize.x;
	float T = 0;
	float B = dd->displaySize.y;
	float proj[4][4] = {
		{2.0f / (R-L),  0.0f,          0.0f, 0.0f},
		{0.0f,          2.0f / (T-B),  0.0f, 0.0f},
		{0.0f,          0.0f,          0.5f, 0.0f},
		{(R+L) / (L-R), (T+B) / (B-T), 0.5f, 1.0f},
	};

	struct d3d12_ui_cb cb = {0};
	memcpy(&cb.proj, proj, sizeof(proj));

	ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(cl, 0, 16, &cb, 0);

	// Set up rendering pipeline
	D3D12_VERTEX_BUFFER_VIEW vbview = {0};
	vbview.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(ctx->vb.res);
	vbview.StrideInBytes = sizeof(MTY_Vtx);
	vbview.SizeInBytes = ctx->vb.len * sizeof(MTY_Vtx);
	ID3D12GraphicsCommandList_IASetVertexBuffers(cl, 0, 1, &vbview);

	D3D12_INDEX_BUFFER_VIEW ibview = {0};
	ibview.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(ctx->ib.res);
	ibview.SizeInBytes = ctx->ib.len * sizeof(uint16_t);
	ibview.Format = DXGI_FORMAT_R16_UINT;
	ID3D12GraphicsCommandList_IASetIndexBuffer(cl, &ibview);

	ID3D12GraphicsCommandList_IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	ID3D12GraphicsCommandList_OMSetBlendFactor(cl, blend_factor);

	// Draw
	uint32_t idxOffset = 0;
	uint32_t vtxOffset = 0;
	void *prev_tex = NULL;

	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		MTY_CmdList *cmdList = &dd->cmdList[x];

		for (uint32_t y = 0; y < cmdList->cmdLength; y++) {
			const MTY_Cmd *pcmd = &cmdList->cmd[y];
			const MTY_Rect *c = &pcmd->clip;

			// Make sure the rect is actually in the viewport
			if (c->left < dd->displaySize.x && c->top < dd->displaySize.y && c->right >= 0 && c->bottom >= 0) {

				// Use the clip to apply scissor
				D3D12_RECT r = {
					.left = lrint(c->left),
					.top = lrint(c->top),
					.right = lrint(c->right),
					.bottom = lrint(c->bottom),
				};
				ID3D12GraphicsCommandList_RSSetScissorRects(cl, 1, &r);

				// Optionally sample from a texture (fonts, images)
				d3d12_ui_set_srv(ctx, _device, cl, cache, pcmd->texture, &prev_tex);

				// Draw indexed
				ID3D12GraphicsCommandList_DrawIndexedInstanced(cl, pcmd->elemCount, 1, pcmd->idxOffset + idxOffset,
					pcmd->vtxOffset + vtxOffset, 0);
			}
		}

		idxOffset += cmdList->idxLength;
		vtxOffset += cmdList->vtxLength;
	}

	return true;
}

void *mty_d3d12_ui_create_texture(struct gfx_ui *gfx_ui, MTY_Device *device, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct d3d12_ui_texture *tex = MTY_Alloc(1, sizeof(struct d3d12_ui_texture));

	ID3D12Device *_device = (ID3D12Device *) device;

	tex->w = width;
	tex->h = height;

	UINT pitch = D3D12_UI_PITCH(width, 4);

	// Upload buffer
	D3D12_HEAP_PROPERTIES hp = {0};
	hp.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC desc = {0};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width =  pitch * height;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.SampleDesc.Count = 1;

	HRESULT e = ID3D12Device_CreateCommittedResource(_device, &hp, D3D12_HEAP_FLAG_NONE, &desc,
		 D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, &tex->buffer);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateCommittedResource' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Copy
	uint8_t *data = NULL;
	e = ID3D12Resource_Map(tex->buffer, 0, NULL, &data);
	if (e != S_OK) {
		MTY_Log("'ID3D12Resource_Map' failed with HRESULT 0x%X", e);
		goto except;
	}

	for (uint32_t y = 0; y < height; y++)
		memcpy(data + y * pitch, (uint8_t *) rgba + y * width * 4, width * 4);

	ID3D12Resource_Unmap(tex->buffer, 0, NULL);

	// Texture
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.SampleDesc.Count = 1;

	e = ID3D12Device_CreateCommittedResource(_device, &hp, D3D12_HEAP_FLAG_NONE, &desc,
		 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL, &IID_ID3D12Resource, &tex->resource);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateCommittedResource' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Shader resource view
	D3D12_DESCRIPTOR_HEAP_DESC dhdesc = {0};
	dhdesc.NumDescriptors = 1;
	dhdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	dhdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	e = ID3D12Device_CreateDescriptorHeap(_device, &dhdesc, &IID_ID3D12DescriptorHeap, &tex->heap);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateDescriptorHeap' failed with HRESULT 0x%X", e);
		goto except;
	}

	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(tex->heap, &tex->dh);

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_dh = {0};
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(tex->heap, &cpu_dh);

	D3D12_SHADER_RESOURCE_VIEW_DESC sdesc = {0};
	sdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	sdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	sdesc.Texture2D.MipLevels = 1;

	ID3D12Device_CreateShaderResourceView(_device, tex->resource, &sdesc, cpu_dh);

	except:

	if (e != S_OK)
		mty_d3d12_ui_destroy_texture(gfx_ui, &tex, device);

	return tex;
}

void mty_d3d12_ui_destroy_texture(struct gfx_ui *gfx_ui, void **texture, MTY_Device *device)
{
	if (!texture || !*texture)
		return;

	struct d3d12_ui_texture *tex = *texture;

	if (tex->heap)
		ID3D12DescriptorHeap_Release(tex->heap);

	if (tex->resource)
		ID3D12Resource_Release(tex->resource);

	if (tex->buffer)
		ID3D12Resource_Release(tex->buffer);

	MTY_Free(tex);
	*texture = NULL;
}

void mty_d3d12_ui_destroy(struct gfx_ui **gfx_ui, MTY_Device *device)
{
	if (!gfx_ui || !*gfx_ui)
		return;

	struct d3d12_ui *ctx = (struct d3d12_ui *) *gfx_ui;

	if (ctx->clear_tex)
		mty_d3d12_ui_destroy_texture(*gfx_ui, &ctx->clear_tex, device);

	if (ctx->vb.res)
		ID3D12Resource_Release(ctx->vb.res);

	if (ctx->ib.res)
		ID3D12Resource_Release(ctx->ib.res);

	if (ctx->pipeline)
		ID3D12PipelineState_Release(ctx->pipeline);

	if (ctx->rs)
		ID3D12RootSignature_Release(ctx->rs);

	MTY_Free(ctx);
	*gfx_ui = NULL;
}
