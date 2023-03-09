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
#include "shaders/psui.h"

static
#include "shaders/vsui.h"

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
	D3D12_ROOT_SIGNATURE_DESC desc = {
		.NumParameters = 2,
		.pParameters = (D3D12_ROOT_PARAMETER []) {{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants.Num32BitValues = 16,
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX,
		}, {
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable.NumDescriptorRanges = 1,
			.DescriptorTable.pDescriptorRanges = &(D3D12_DESCRIPTOR_RANGE) {
				.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
				.NumDescriptors = 1,
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		}},
		.NumStaticSamplers = 1,
		.pStaticSamplers = &(D3D12_STATIC_SAMPLER_DESC) {
			.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
			.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		},
		.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS,
	};

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
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
		.VS.pShaderBytecode = vsui,
		.VS.BytecodeLength = sizeof(vsui),
		.PS.pShaderBytecode = psui,
		.PS.BytecodeLength = sizeof(psui),
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.pRootSignature = ctx->rs,
		.SampleMask = UINT_MAX,
		.NumRenderTargets = 1,
		.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM,
		.SampleDesc.Count = 1,
		.InputLayout.pInputElementDescs = (D3D12_INPUT_ELEMENT_DESC []) {
			{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(MTY_Vtx, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(MTY_Vtx, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(MTY_Vtx, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		},
		.InputLayout.NumElements = 3,
		.BlendState.RenderTarget[0] = {
			.BlendEnable = TRUE,
			.SrcBlend = D3D12_BLEND_SRC_ALPHA,
			.DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
			.BlendOp = D3D12_BLEND_OP_ADD,
			.SrcBlendAlpha = D3D12_BLEND_ONE,
			.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
			.BlendOpAlpha = D3D12_BLEND_OP_ADD,
			.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
		},
		.RasterizerState = {
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
			.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
			.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
			.DepthClipEnable = TRUE,
			.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
		},
		.DepthStencilState = {
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS,
			.FrontFace = {
				.StencilFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp = D3D12_STENCIL_OP_KEEP,
				.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
			},
			.BackFace = {
				.StencilFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp = D3D12_STENCIL_OP_KEEP,
				.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
			},
		},
	};

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

		D3D12_HEAP_PROPERTIES hp = {
			.Type = D3D12_HEAP_TYPE_UPLOAD,
		};

		D3D12_RESOURCE_DESC rd = {
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Width = (len + incr) * element_size,
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.SampleDesc.Count = 1,
		};

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
		D3D12_RESOURCE_BARRIER rb = {
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.Transition.pResource = tex->resource,
			.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST,
		};

		ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &rb);

		D3D12_TEXTURE_COPY_LOCATION tsrc = {
			.pResource = tex->buffer,
			.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
			.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.PlacedFootprint.Footprint.Width = tex->w,
			.PlacedFootprint.Footprint.Height = tex->h,
			.PlacedFootprint.Footprint.Depth = 1,
			.PlacedFootprint.Footprint.RowPitch = D3D12_UI_PITCH(tex->w, 4),
		};

		D3D12_TEXTURE_COPY_LOCATION tdest = {
			.pResource = tex->resource,
			.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
		};

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
	D3D12_VIEWPORT vp = {
		.Width = dd->displaySize.x,
		.Height = dd->displaySize.y,
		.MaxDepth = 1,
	};

	ID3D12GraphicsCommandList_RSSetViewports(cl, 1, &vp);

	// Set render target (wraps the texture)
	if (_dest) {
		ID3D12GraphicsCommandList_OMSetRenderTargets(cl, 1, _dest, FALSE, NULL);

		// Clear render target to black
		if (dd->clear) {
			const float color[4] = {0, 0, 0, 1};
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
	struct d3d12_ui_cb cb = {
		.proj = {
			{2.0f / (R-L),  0.0f,          0.0f, 0.0f},
			{0.0f,          2.0f / (T-B),  0.0f, 0.0f},
			{0.0f,          0.0f,          0.5f, 0.0f},
			{(R+L) / (L-R), (T+B) / (B-T), 0.5f, 1.0f},
		},
	};

	ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(cl, 0, 16, &cb, 0);

	// Set up rendering pipeline
	D3D12_VERTEX_BUFFER_VIEW vbview = {
		.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(ctx->vb.res),
		.StrideInBytes = sizeof(MTY_Vtx),
		.SizeInBytes = ctx->vb.len * sizeof(MTY_Vtx),
	};

	ID3D12GraphicsCommandList_IASetVertexBuffers(cl, 0, 1, &vbview);

	D3D12_INDEX_BUFFER_VIEW ibview = {
		ibview.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(ctx->ib.res),
		ibview.SizeInBytes = ctx->ib.len * sizeof(uint16_t),
		ibview.Format = DXGI_FORMAT_R16_UINT,
	};

	ID3D12GraphicsCommandList_IASetIndexBuffer(cl, &ibview);
	ID3D12GraphicsCommandList_IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const float blend_factor[4] = {0};
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
	D3D12_HEAP_PROPERTIES hp = {
		.Type = D3D12_HEAP_TYPE_UPLOAD,
	};

	D3D12_RESOURCE_DESC desc = {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width =  pitch * height,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.SampleDesc.Count = 1,
	};

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
	D3D12_DESCRIPTOR_HEAP_DESC dhdesc = {
		.NumDescriptors = 1,
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	};

	e = ID3D12Device_CreateDescriptorHeap(_device, &dhdesc, &IID_ID3D12DescriptorHeap, &tex->heap);
	if (e != S_OK) {
		MTY_Log("'ID3D12Device_CreateDescriptorHeap' failed with HRESULT 0x%X", e);
		goto except;
	}

	ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(tex->heap, &tex->dh);

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_dh = {0};
	ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(tex->heap, &cpu_dh);

	D3D12_SHADER_RESOURCE_VIEW_DESC sdesc = {
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D.MipLevels = 1,
	};

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
