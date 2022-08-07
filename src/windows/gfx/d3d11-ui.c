// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ui.h"
GFX_UI_PROTOTYPES(_d3d11_)

#include <stdio.h>
#include <math.h>

#define COBJMACROS
#include <d3d11.h>

static
#include "shaders/d3d11/psui.h"

static
#include "shaders/d3d11/vsui.h"

struct d3d11_ui_buffer {
	ID3D11Buffer *b;
	ID3D11Resource *res;
	uint32_t len;
};

struct d3d11_ui {
	struct d3d11_ui_buffer vb;
	struct d3d11_ui_buffer ib;
	ID3D11VertexShader *vs;
	ID3D11InputLayout *il;
	ID3D11Buffer *cb;
	ID3D11Resource *cb_res;
	ID3D11PixelShader *ps;
	ID3D11SamplerState *sampler;
	ID3D11RasterizerState *rs;
	ID3D11BlendState *bs;
	ID3D11DepthStencilState *dss;
};

struct d3d11_ui_cb {
	float proj[4][4];
};

struct gfx_ui *mty_d3d11_ui_create(MTY_Device *device)
{
	struct d3d11_ui *ctx = MTY_Alloc(1, sizeof(struct d3d11_ui));
	ID3D11Device *_device = (ID3D11Device *) device;

	// Create vertex, pixel shaders from precompiled data from headers
	HRESULT e = ID3D11Device_CreateVertexShader(_device, vsui, sizeof(vsui), NULL, &ctx->vs);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateVertexShader' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Device_CreatePixelShader(_device, psui, sizeof(psui), NULL, &ctx->ps);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreatePixelShader' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Input layout describing the shape of the vertex buffer
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(MTY_Vtx, pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(MTY_Vtx, uv),  D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",	 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(MTY_Vtx, col), D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	e = ID3D11Device_CreateInputLayout(_device, layout, 3, vsui, sizeof(vsui), &ctx->il);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateInputLayout' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Pre create a constant buffer used for storing the vertex shader's projection data
	D3D11_BUFFER_DESC desc = {0};
	desc.ByteWidth = sizeof(struct d3d11_ui_cb);
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	e = ID3D11Device_CreateBuffer(_device, &desc, NULL, &ctx->cb);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Buffer_QueryInterface(ctx->cb, &IID_ID3D11Resource, &ctx->cb_res);
	if (e != S_OK) {
		MTY_Log("'ID3D11Buffer_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Blend state
	D3D11_BLEND_DESC bdesc = {0};
	bdesc.AlphaToCoverageEnable = false;
	bdesc.RenderTarget[0].BlendEnable = true;
	bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	e = ID3D11Device_CreateBlendState(_device, &bdesc, &ctx->bs);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBlendState' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Rastersizer state enabling scissoring
	D3D11_RASTERIZER_DESC rdesc = {0};
	rdesc.FillMode = D3D11_FILL_SOLID;
	rdesc.CullMode = D3D11_CULL_NONE;
	rdesc.ScissorEnable = true;
	rdesc.DepthClipEnable = true;
	e = ID3D11Device_CreateRasterizerState(_device, &rdesc, &ctx->rs);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateRasterizerState' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Depth stencil state
	D3D11_DEPTH_STENCIL_DESC sdesc = {0};
	sdesc.DepthEnable = false;
	sdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	sdesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	sdesc.StencilEnable = false;
	sdesc.FrontFace.StencilFailOp = sdesc.FrontFace.StencilDepthFailOp
		= sdesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	sdesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	sdesc.BackFace = sdesc.FrontFace;
	e = ID3D11Device_CreateDepthStencilState(_device, &sdesc, &ctx->dss);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateDepthStencilState' failed with HRESULT 0x%X", e);
		goto except;
	}

	// Sampler state for font texture
	D3D11_SAMPLER_DESC samdesc = {0};
	samdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samdesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	e = ID3D11Device_CreateSamplerState(_device, &samdesc, &ctx->sampler);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateSamplerState' failed with HRESULT 0x%X", e);
		goto except;
	}

	except:

	if (e != S_OK)
		mty_d3d11_ui_destroy((struct gfx_ui **) &ctx, device);

	return (struct gfx_ui *) ctx;
}

static HRESULT d3d11_ui_resize_buffer(ID3D11Device *device, struct d3d11_ui_buffer *buf,
	uint32_t len, uint32_t incr, uint32_t element_size, enum D3D11_BIND_FLAG bind_flag)
{
	if (buf->len < len) {
		if (buf->res) {
			ID3D11Resource_Release(buf->res);
			buf->res = NULL;
		}

		if (buf->b) {
			ID3D11Buffer_Release(buf->b);
			buf->b = NULL;
		}

		D3D11_BUFFER_DESC desc = {0};
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = (len + incr) * element_size;
		desc.BindFlags = bind_flag;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT e = ID3D11Device_CreateBuffer(device, &desc, NULL, &buf->b);
		if (e != S_OK) {
			MTY_Log("'ID3D11Device_CreateBuffer' failed with HRESULT 0x%X", e);
			return e;
		}

		e = ID3D11Buffer_QueryInterface(buf->b, &IID_ID3D11Resource, &buf->res);
		if (e != S_OK) {
			MTY_Log("'ID3D11Buffer_QueryInterface' failed with HRESULT 0x%X", e);
			return e;
		}

		buf->len = len + incr;
	}

	return S_OK;
}

bool mty_d3d11_ui_render(struct gfx_ui *gfx_ui, MTY_Device *device, MTY_Context *context,
	const MTY_DrawData *dd, MTY_Hash *cache, MTY_Surface *dest)
{
	struct d3d11_ui *ctx = (struct d3d11_ui *) gfx_ui;
	ID3D11Device *_device = (ID3D11Device *) device;
	ID3D11DeviceContext *_context = (ID3D11DeviceContext *) context;
	ID3D11Texture2D *_dest = (ID3D11Texture2D *) dest;

	ID3D11Resource *tex_res = NULL;
	ID3D11RenderTargetView *rtv = NULL;

	// Prevent rendering under invalid scenarios
	if (dd->displaySize.x <= 0 || dd->displaySize.y <= 0 || dd->cmdListLength == 0)
		return false;

	// Resize vertex and index buffers if necessary
	HRESULT e = d3d11_ui_resize_buffer(_device, &ctx->vb, dd->vtxTotalLength, GFX_UI_VTX_INCR, sizeof(MTY_Vtx),
		D3D11_BIND_VERTEX_BUFFER);
	if (e != S_OK)
		goto except;

	e = d3d11_ui_resize_buffer(_device, &ctx->ib, dd->idxTotalLength, GFX_UI_IDX_INCR, sizeof(uint16_t),
		D3D11_BIND_INDEX_BUFFER);
	if (e != S_OK)
		goto except;

	// Map both vertex and index buffers and bulk copy the data
	D3D11_MAPPED_SUBRESOURCE vtx_map = {0};
	e = ID3D11DeviceContext_Map(_context, ctx->vb.res, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_map);
	if (e != S_OK) {
		MTY_Log("'ID3D11DeviceContext_Map' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_MAPPED_SUBRESOURCE idx_map = {0};
	e = ID3D11DeviceContext_Map(_context, ctx->ib.res, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_map);
	if (e != S_OK) {
		MTY_Log("'ID3D11DeviceContext_Map' failed with HRESULT 0x%X", e);
		goto except;
	}

	MTY_Vtx *vtx_dst = (MTY_Vtx *) vtx_map.pData;
	uint16_t *idx_dst = (uint16_t *) idx_map.pData;

	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		MTY_CmdList *cmdList = &dd->cmdList[x];

		memcpy(vtx_dst, cmdList->vtx, cmdList->vtxLength * sizeof(MTY_Vtx));
		memcpy(idx_dst, cmdList->idx, cmdList->idxLength * sizeof(uint16_t));

		vtx_dst += cmdList->vtxLength;
		idx_dst += cmdList->idxLength;
	}

	ID3D11DeviceContext_Unmap(_context, ctx->ib.res, 0);
	ID3D11DeviceContext_Unmap(_context, ctx->vb.res, 0);

	// Update the vertex shader's projection data based on the current display size
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

	D3D11_MAPPED_SUBRESOURCE cb_map = {0};
	e = ID3D11DeviceContext_Map(_context, ctx->cb_res, 0, D3D11_MAP_WRITE_DISCARD, 0, &cb_map);
	if (e != S_OK)
		goto except;

	struct d3d11_ui_cb *cb = (struct d3d11_ui_cb *) cb_map.pData;
	memcpy(&cb->proj, proj, sizeof(proj));
	ID3D11DeviceContext_Unmap(_context, ctx->cb_res, 0);

	// Set render target (wraps the texture)
	if (_dest) {
		e = ID3D11Texture2D_QueryInterface(_dest, &IID_ID3D11Resource, &tex_res);
		if (e != S_OK) {
			MTY_Log("'ID3D11Texture2D_QueryInterface' failed with HRESULT 0x%X", e);
			goto except;
		}

		e = ID3D11Device_CreateRenderTargetView(_device, tex_res, NULL, &rtv);
		if (e != S_OK) {
			MTY_Log("'ID3D11Device_CreateRenderTargetView' failed with HRESULT 0x%X", e);
			goto except;
		}

		ID3D11DeviceContext_OMSetRenderTargets(_context, 1, &rtv, NULL);
	}

	// Set viewport based on display size
	D3D11_VIEWPORT vp = {0};
	vp.Width = dd->displaySize.x;
	vp.Height = dd->displaySize.y;
	vp.MaxDepth = 1.0f;
	ID3D11DeviceContext_RSSetViewports(_context, 1, &vp);

	// Clear render target to black
	if (rtv && dd->clear) {
		FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		ID3D11DeviceContext_ClearRenderTargetView(_context, rtv, clear_color);
	}

	// Set up rendering pipeline
	uint32_t stride = sizeof(MTY_Vtx);
	uint32_t offset = 0;
	ID3D11DeviceContext_IASetInputLayout(_context, ctx->il);
	ID3D11DeviceContext_IASetVertexBuffers(_context, 0, 1, &ctx->vb.b, &stride, &offset);
	ID3D11DeviceContext_IASetIndexBuffer(_context, ctx->ib.b, DXGI_FORMAT_R16_UINT, 0);
	ID3D11DeviceContext_IASetPrimitiveTopology(_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D11DeviceContext_VSSetShader(_context, ctx->vs, NULL, 0);
	ID3D11DeviceContext_VSSetConstantBuffers(_context, 0, 1, &ctx->cb);
	ID3D11DeviceContext_PSSetShader(_context, ctx->ps, NULL, 0);
	ID3D11DeviceContext_PSSetSamplers(_context, 0, 1, &ctx->sampler);

	const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	ID3D11DeviceContext_OMSetBlendState(_context, ctx->bs, blend_factor, 0xFFFFFFFF);
	ID3D11DeviceContext_OMSetDepthStencilState(_context, ctx->dss, 0);
	ID3D11DeviceContext_RSSetState(_context, ctx->rs);

	// Draw
	uint32_t idxOffset = 0;
	uint32_t vtxOffset = 0;

	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		MTY_CmdList *cmdList = &dd->cmdList[x];

		for (uint32_t y = 0; y < cmdList->cmdLength; y++) {
			const MTY_Cmd *pcmd = &cmdList->cmd[y];
			const MTY_Rect *c = &pcmd->clip;

			// Make sure the rect is actually in the viewport
			if (c->left < dd->displaySize.x && c->top < dd->displaySize.y && c->right >= 0 && c->bottom >= 0) {

				// Use the clip to apply scissor
				D3D11_RECT r = {
					.left = lrint(c->left),
					.top = lrint(c->top),
					.right = lrint(c->right),
					.bottom = lrint(c->bottom),
				};
				ID3D11DeviceContext_RSSetScissorRects(_context, 1, &r);

				// Optionally sample from a texture (fonts, images)
				ID3D11ShaderResourceView *srv = !pcmd->texture ? NULL : MTY_HashGetInt(cache, pcmd->texture);
				ID3D11DeviceContext_PSSetShaderResources(_context, 0, 1, &srv);

				// Draw indexed
				ID3D11DeviceContext_DrawIndexed(_context, pcmd->elemCount, pcmd->idxOffset + idxOffset,
					pcmd->vtxOffset + vtxOffset);
			}
		}

		idxOffset += cmdList->idxLength;
		vtxOffset += cmdList->vtxLength;
	}

	except:

	if (rtv)
		ID3D11RenderTargetView_Release(rtv);

	if (tex_res)
		ID3D11Resource_Release(tex_res);

	return e == S_OK;
}

void *mty_d3d11_ui_create_texture(struct gfx_ui *gfx_ui, MTY_Device *device, const void *rgba,
	uint32_t width, uint32_t height)
{
	ID3D11Device *_device = (ID3D11Device *) device;

	ID3D11Texture2D *tex = NULL;
	ID3D11Resource *res = NULL;
	ID3D11ShaderResourceView *srv = NULL;

	D3D11_TEXTURE2D_DESC tdesc = {0};
	tdesc.Width = width;
	tdesc.Height = height;
	tdesc.MipLevels = 1;
	tdesc.ArraySize = 1;
	tdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tdesc.SampleDesc.Count = 1;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA subres = {0};
	subres.pSysMem = rgba;
	subres.SysMemPitch = tdesc.Width * 4;
	HRESULT e = ID3D11Device_CreateTexture2D(_device, &tdesc, &subres, &tex);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateTexture2D' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Texture2D_QueryInterface(tex, &IID_ID3D11Resource, &res);
	if (e != S_OK) {
		MTY_Log("'ID3D11Texture2D_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {0};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = tdesc.MipLevels;
	e = ID3D11Device_CreateShaderResourceView(_device, res, &srvDesc, &srv);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateShaderResourceView' failed with HRESULT 0x%X", e);
		goto except;
	}

	except:

	if (res)
		ID3D11Resource_Release(res);

	if (tex)
		ID3D11Texture2D_Release(tex);

	if (e != S_OK && srv) {
		ID3D11ShaderResourceView_Release(srv);
		srv = NULL;
	}

	return srv;
}

void mty_d3d11_ui_destroy_texture(struct gfx_ui *gfx_ui, void **texture, MTY_Device *device)
{
	if (!texture || !*texture)
		return;

	ID3D11ShaderResourceView *srv = *texture;
	ID3D11ShaderResourceView_Release(srv);

	*texture = NULL;
}

void mty_d3d11_ui_destroy(struct gfx_ui **gfx_ui, MTY_Device *device)
{
	if (!gfx_ui || !*gfx_ui)
		return;

	struct d3d11_ui *ctx = (struct d3d11_ui *) *gfx_ui;

	if (ctx->sampler)
		ID3D11SamplerState_Release(ctx->sampler);

	if (ctx->vb.res)
		ID3D11Resource_Release(ctx->vb.res);

	if (ctx->ib.res)
		ID3D11Resource_Release(ctx->ib.res);

	if (ctx->ib.b)
		ID3D11Buffer_Release(ctx->ib.b);

	if (ctx->vb.b)
		ID3D11Buffer_Release(ctx->vb.b);

	if (ctx->bs)
		ID3D11BlendState_Release(ctx->bs);

	if (ctx->dss)
		ID3D11DepthStencilState_Release(ctx->dss);

	if (ctx->rs)
		ID3D11RasterizerState_Release(ctx->rs);

	if (ctx->ps)
		ID3D11PixelShader_Release(ctx->ps);

	if (ctx->cb_res)
		ID3D11Resource_Release(ctx->cb_res);

	if (ctx->cb)
		ID3D11Buffer_Release(ctx->cb);

	if (ctx->il)
		ID3D11InputLayout_Release(ctx->il);

	if (ctx->vs)
		ID3D11VertexShader_Release(ctx->vs);

	MTY_Free(ctx);
	*gfx_ui = NULL;
}
