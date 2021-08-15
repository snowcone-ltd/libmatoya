// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod.h"
GFX_PROTOTYPES(_d3d11_)

#define COBJMACROS
#include <d3d11.h>

#include "gfx/viewport.h"

static
#include "shaders/d3d11/ps.h"

static
#include "shaders/d3d11/vs.h"

#define D3D11_NUM_STAGING 3

struct d3d11_psvars {
	float width;
	float height;
	float vp_height;
	uint32_t filter;
	uint32_t effect;
	uint32_t format;
	uint32_t rotation;
	uint32_t __pad[1]; // Constant buffers must be in increments of 16 bytes
};

struct d3d11_res {
	DXGI_FORMAT format;
	ID3D11Texture2D *texture;
	ID3D11Resource *resource;
	ID3D11ShaderResourceView *srv;
	uint32_t width;
	uint32_t height;
};

struct d3d11 {
	MTY_ColorFormat format;
	struct d3d11_res staging[D3D11_NUM_STAGING];
	ID3D11VertexShader *vs;
	ID3D11PixelShader *ps;
	ID3D11Buffer *vb;
	ID3D11Buffer *ib;
	ID3D11Buffer *psb;
	ID3D11Resource *psbres;
	ID3D11InputLayout *il;
	ID3D11SamplerState *ss_nearest;
	ID3D11SamplerState *ss_linear;
	ID3D11RasterizerState *rs;
	ID3D11DepthStencilState *dss;
};

struct gfx *mty_d3d11_create(MTY_Device *device)
{
	struct d3d11 *ctx = MTY_Alloc(1, sizeof(struct d3d11));
	ID3D11Device *_device = (ID3D11Device *) device;

	HRESULT e = ID3D11Device_CreateVertexShader(_device, vs, sizeof(vs), NULL, &ctx->vs);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateVertexShader' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Device_CreatePixelShader(_device, ps, sizeof(ps), NULL, &ctx->ps);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreatePixelShader' failed with HRESULT 0x%X", e);
		goto except;
	}

	float vertex_data[] = {
		-1.0f, -1.0f,  // position0 (bottom-left)
		 0.0f,  1.0f,  // texcoord0
		-1.0f,  1.0f,  // position1 (top-left)
		 0.0f,  0.0f,  // texcoord1
		 1.0f,  1.0f,  // position2 (top-right)
		 1.0f,  0.0f,  // texcoord2
		 1.0f, -1.0f,  // position3 (bottom-right)
		 1.0f,  1.0f   // texcoord3
	};

	D3D11_BUFFER_DESC bd = {0};
	bd.ByteWidth = sizeof(vertex_data);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = {0};
	srd.pSysMem = vertex_data;
	e = ID3D11Device_CreateBuffer(_device, &bd, &srd, &ctx->vb);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	DWORD index_data[] = {
		0, 1, 2,
		2, 3, 0
	};

	D3D11_BUFFER_DESC ibd = {0};
	ibd.ByteWidth = sizeof(index_data);
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA isrd = {0};
	isrd.pSysMem = index_data;
	e = ID3D11Device_CreateBuffer(_device, &ibd, &isrd, &ctx->ib);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_BUFFER_DESC psbd = {0};
	psbd.ByteWidth = sizeof(struct d3d11_psvars);
	psbd.Usage = D3D11_USAGE_DYNAMIC;
	psbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	psbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	e = ID3D11Device_CreateBuffer(_device, &psbd, NULL, &ctx->psb);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = ID3D11Buffer_QueryInterface(ctx->psb, &IID_ID3D11Resource, &ctx->psbres);
	if (e != S_OK) {
		MTY_Log("'ID3D11Buffer_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_INPUT_ELEMENT_DESC ied[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 2 * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	e = ID3D11Device_CreateInputLayout(_device, ied, 2, vs, sizeof(vs), &ctx->il);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateInputLayout' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_SAMPLER_DESC sdesc = {0};
	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	e = ID3D11Device_CreateSamplerState(_device, &sdesc, &ctx->ss_nearest);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateSamplerState' failed with HRESULT 0x%X", e);
		goto except;
	}

	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	e = ID3D11Device_CreateSamplerState(_device, &sdesc, &ctx->ss_linear);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateSamplerState' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_RASTERIZER_DESC rdesc = {0};
	rdesc.FillMode = D3D11_FILL_SOLID;
	rdesc.CullMode = D3D11_CULL_NONE;
	e = ID3D11Device_CreateRasterizerState(_device, &rdesc, &ctx->rs);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateRasterizerState' failed with HRESULT 0x%X", e);
		goto except;
	}

	D3D11_DEPTH_STENCIL_DESC dssdesc = {0};
	e = ID3D11Device_CreateDepthStencilState(_device, &dssdesc, &ctx->dss);
	if (e != S_OK) {
		MTY_Log("'ID3D11Device_CreateDepthStencilState' failed with HRESULT 0x%X", e);
		goto except;
	}

	except:

	if (e != S_OK)
		mty_d3d11_destroy((struct gfx **) &ctx);

	return (struct gfx *) ctx;
}

static void d3d11_destroy_resource(struct d3d11_res *res)
{
	if (res->srv)
		ID3D11ShaderResourceView_Release(res->srv);

	if (res->resource)
		ID3D11Resource_Release(res->resource);

	if (res->texture)
		ID3D11Texture2D_Release(res->texture);

	memset(res, 0, sizeof(struct d3d11_res));
}

static HRESULT d3d11_refresh_resource(struct d3d11_res *res, ID3D11Device *device,
	DXGI_FORMAT format, uint32_t width, uint32_t height)
{
	HRESULT e = S_OK;

	if (!res->texture || !res->srv || !res->resource || width != res->width ||
		height != res->height || format != res->format)
	{
		d3d11_destroy_resource(res);

		D3D11_TEXTURE2D_DESC desc = {0};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		e = ID3D11Device_CreateTexture2D(device, &desc, NULL, &res->texture);
		if (e != S_OK) {
			MTY_Log("'ID3D11Device_CreateTexture2D' failed with HRESULT 0x%X", e);
			goto except;
		}

		e = ID3D11Texture2D_QueryInterface(res->texture, &IID_ID3D11Resource, &res->resource);
		if (e != S_OK) {
			MTY_Log("'ID3D11Texture2D_QueryInterface' failed with HRESULT 0x%X", e);
			goto except;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {0};
		srvd.Format = format;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;

		e = ID3D11Device_CreateShaderResourceView(device, res->resource, &srvd, &res->srv);
		if (e != S_OK) {
			MTY_Log("'ID3D11Device_CreateShaderResourceView' failed with HRESULT 0x%X", e);
			goto except;
		}

		res->width = width;
		res->height = height;
		res->format = format;
	}

	except:

	if (e != S_OK)
		d3d11_destroy_resource(res);

	return e;
}

static HRESULT d3d11_crop_copy(ID3D11DeviceContext *context, ID3D11Resource *texture,
	const uint8_t *image, uint32_t w, uint32_t h, int32_t fullWidth, uint8_t bpp)
{
	D3D11_MAPPED_SUBRESOURCE res;
	HRESULT e = ID3D11DeviceContext_Map(context, texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
	if (e != S_OK) {
		MTY_Log("'ID3D11DeviceContext_Map' failed with HRESULT 0x%X", e);
		return e;
	}

	for (uint32_t y = 0; y < h; y++)
		memcpy((uint8_t *) res.pData + (y * res.RowPitch), image + (y * fullWidth * bpp), w * bpp);

	ID3D11DeviceContext_Unmap(context, texture, 0);

	return e;
}

static HRESULT d3d11_reload_textures(struct d3d11 *ctx, ID3D11Device *device, ID3D11DeviceContext *context,
	const void *image, const MTY_RenderDesc *desc)
{
	switch (desc->format) {
		case MTY_COLOR_FORMAT_BGRA:
		case MTY_COLOR_FORMAT_AYUV:
		case MTY_COLOR_FORMAT_BGR565:
		case MTY_COLOR_FORMAT_BGRA5551:
		case MTY_COLOR_FORMAT_RGBA: {
			DXGI_FORMAT format =
				desc->format == MTY_COLOR_FORMAT_BGR565   ? DXGI_FORMAT_B5G6R5_UNORM   :
				desc->format == MTY_COLOR_FORMAT_BGRA5551 ? DXGI_FORMAT_B5G5R5A1_UNORM :
				desc->format == MTY_COLOR_FORMAT_RGBA     ? DXGI_FORMAT_R8G8B8A8_UNORM :
				DXGI_FORMAT_B8G8R8A8_UNORM;
			uint8_t bpp =
				desc->format == MTY_COLOR_FORMAT_BGRA ? sizeof(uint32_t) :
				desc->format == MTY_COLOR_FORMAT_AYUV ? sizeof(uint32_t) :
				desc->format == MTY_COLOR_FORMAT_RGBA ? sizeof(uint32_t) :
				sizeof(uint16_t);

			// BGRA
			HRESULT e = d3d11_refresh_resource(&ctx->staging[0], device, format, desc->cropWidth, desc->cropHeight);
			if (e != S_OK) return e;

			e = d3d11_crop_copy(context, ctx->staging[0].resource, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, bpp);
			if (e != S_OK) return e;
			break;
		}
		case MTY_COLOR_FORMAT_NV12: {
			// Y
			HRESULT e = d3d11_refresh_resource(&ctx->staging[0], device, DXGI_FORMAT_R8_UNORM, desc->cropWidth, desc->cropHeight);
			if (e != S_OK) return e;

			e = d3d11_crop_copy(context, ctx->staging[0].resource, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, 1);
			if (e != S_OK) return e;

			// UV
			e = d3d11_refresh_resource(&ctx->staging[1], device, DXGI_FORMAT_R8G8_UNORM, desc->cropWidth / 2, desc->cropHeight / 2);
			if (e != S_OK) return e;

			e = d3d11_crop_copy(context, ctx->staging[1].resource, (uint8_t *) image + desc->imageWidth * desc->imageHeight, desc->cropWidth / 2, desc->cropHeight / 2, desc->imageWidth / 2, 2);
			if (e != S_OK) return e;
			break;
		}
		case MTY_COLOR_FORMAT_I420:
		case MTY_COLOR_FORMAT_I444: {
			uint32_t div = desc->format == MTY_COLOR_FORMAT_I420 ? 2 : 1;

			// Y
			HRESULT e = d3d11_refresh_resource(&ctx->staging[0], device, DXGI_FORMAT_R8_UNORM, desc->cropWidth, desc->cropHeight);
			if (e != S_OK) return e;

			e = d3d11_crop_copy(context, ctx->staging[0].resource, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, 1);
			if (e != S_OK) return e;

			// U
			uint8_t *p = (uint8_t *) image + desc->imageWidth * desc->imageHeight;
			e = d3d11_refresh_resource(&ctx->staging[1], device, DXGI_FORMAT_R8_UNORM, desc->cropWidth / div, desc->cropHeight / div);
			if (e != S_OK) return e;

			e = d3d11_crop_copy(context, ctx->staging[1].resource, p, desc->cropWidth / div, desc->cropHeight / div, desc->imageWidth / div, 1);
			if (e != S_OK) return e;

			// V
			p += (desc->imageWidth / div) * (desc->imageHeight / div);
			e = d3d11_refresh_resource(&ctx->staging[2], device, DXGI_FORMAT_R8_UNORM, desc->cropWidth / div, desc->cropHeight / div);
			if (e != S_OK) return e;

			e = d3d11_crop_copy(context, ctx->staging[2].resource, p, desc->cropWidth / div, desc->cropHeight / div, desc->imageWidth / div, 1);
			if (e != S_OK) return e;
			break;
		}
	}

	return S_OK;
}

bool mty_d3d11_render(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest)
{
	struct d3d11 *ctx = (struct d3d11 *) gfx;
	ID3D11Device *_device = (ID3D11Device *) device;
	ID3D11DeviceContext *_context = (ID3D11DeviceContext *) context;
	ID3D11Texture2D *_dest = (ID3D11Texture2D *) dest;

	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	// Refresh textures and load texture data
	// If format == MTY_COLOR_FORMAT_UNKNOWN, texture refreshing/loading is skipped and the previous frame is rendered
	HRESULT e = d3d11_reload_textures(ctx, _device, _context, image, desc);
	if (e != S_OK)
		return false;

	// Viewport
	D3D11_VIEWPORT vp = {0};
	mty_viewport(desc->rotation, desc->cropWidth, desc->cropHeight,
		desc->viewWidth, desc->viewHeight, desc->aspectRatio, desc->scale,
		&vp.TopLeftX, &vp.TopLeftY, &vp.Width, &vp.Height);

	ID3D11DeviceContext_RSSetViewports(_context, 1, &vp);

	// Begin render pass (set destination texture if available)
	ID3D11Resource *rtvresource = NULL;
	ID3D11RenderTargetView *rtv = NULL;

	if (_dest) {
		e = ID3D11Texture2D_QueryInterface(_dest, &IID_ID3D11Resource, &rtvresource);
		if (e != S_OK) {
			MTY_Log("'ID3D11Texture2D_QueryInterface' failed with HRESULT 0x%X", e);
			goto except;
		}

		e = ID3D11Device_CreateRenderTargetView(_device, rtvresource, NULL, &rtv);
		if (e != S_OK) {
			MTY_Log("'ID3D11Device_CreateRenderTargetView' failed with HRESULT 0x%X", e);
			goto except;
		}

		ID3D11DeviceContext_OMSetRenderTargets(_context, 1, &rtv, NULL);

		FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		ID3D11DeviceContext_ClearRenderTargetView(_context, rtv, clear_color);
	}

	// Vertex shader
	UINT stride = 4 * sizeof(float);
	UINT offset = 0;

	ID3D11DeviceContext_VSSetShader(_context, ctx->vs, NULL, 0);
	ID3D11DeviceContext_IASetVertexBuffers(_context, 0, 1, &ctx->vb, &stride, &offset);
	ID3D11DeviceContext_IASetIndexBuffer(_context, ctx->ib, DXGI_FORMAT_R32_UINT, 0);
	ID3D11DeviceContext_IASetInputLayout(_context, ctx->il);
	ID3D11DeviceContext_IASetPrimitiveTopology(_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D11DeviceContext_OMSetBlendState(_context, NULL, NULL, 0xFFFFFFFF);
	ID3D11DeviceContext_OMSetDepthStencilState(_context, ctx->dss, 0);
	ID3D11DeviceContext_RSSetState(_context, ctx->rs);

	// Pixel shader
	ID3D11DeviceContext_PSSetShader(_context, ctx->ps, NULL, 0);
	ID3D11DeviceContext_PSSetSamplers(_context, 0, 1, desc->filter == MTY_FILTER_NEAREST ? &ctx->ss_nearest : &ctx->ss_linear);

	for (uint8_t x = 0; x < D3D11_NUM_STAGING; x++)
		if (ctx->staging[x].srv)
			ID3D11DeviceContext_PSSetShaderResources(_context, x, 1, &ctx->staging[x].srv);

	struct d3d11_psvars cb = {0};
	cb.width = (float) desc->cropWidth;
	cb.height = (float) desc->cropHeight;
	cb.vp_height = (float) vp.Height;
	cb.filter = desc->filter;
	cb.effect = desc->effect;
	cb.format = ctx->format;
	cb.rotation = desc->rotation;

	D3D11_MAPPED_SUBRESOURCE res = {0};
	e = ID3D11DeviceContext_Map(_context, ctx->psbres, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
	if (e != S_OK) {
		MTY_Log("'ID3D11DeviceContext_Map' failed with HRESULT 0x%X", e);
		goto except;
	}

	memcpy(res.pData, &cb, sizeof(struct d3d11_psvars));
	ID3D11DeviceContext_Unmap(_context, ctx->psbres, 0);
	ID3D11DeviceContext_PSSetConstantBuffers(_context, 0, 1, &ctx->psb);

	// Draw
	ID3D11DeviceContext_DrawIndexed(_context, 6, 0, 0);

	except:

	if (rtv)
		ID3D11RenderTargetView_Release(rtv);

	if (rtvresource)
		ID3D11Resource_Release(rtvresource);

	return e == S_OK;
}

void mty_d3d11_destroy(struct gfx **gfx)
{
	if (!gfx || !*gfx)
		return;

	struct d3d11 *ctx = (struct d3d11 *) *gfx;

	for (uint8_t x = 0; x < D3D11_NUM_STAGING; x++)
		d3d11_destroy_resource(&ctx->staging[x]);

	if (ctx->rs)
		ID3D11RasterizerState_Release(ctx->rs);

	if (ctx->dss)
		ID3D11DepthStencilState_Release(ctx->dss);

	if (ctx->ss_linear)
		ID3D11SamplerState_Release(ctx->ss_linear);

	if (ctx->ss_nearest)
		ID3D11SamplerState_Release(ctx->ss_nearest);

	if (ctx->il)
		ID3D11InputLayout_Release(ctx->il);

	if (ctx->psbres)
		ID3D11Resource_Release(ctx->psbres);

	if (ctx->psb)
		ID3D11Buffer_Release(ctx->psb);

	if (ctx->ib)
		ID3D11Buffer_Release(ctx->ib);

	if (ctx->vb)
		ID3D11Buffer_Release(ctx->vb);

	if (ctx->ps)
		ID3D11PixelShader_Release(ctx->ps);

	if (ctx->vs)
		ID3D11VertexShader_Release(ctx->vs);

	MTY_Free(ctx);
	*gfx = NULL;
}


// State

struct d3d11_state {
	UINT sr_count;
	UINT vp_count;
	D3D11_RECT sr[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	D3D11_VIEWPORT vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	ID3D11RasterizerState *rs;
	ID3D11BlendState *bs;
	FLOAT bf[4];
	UINT mask;
	UINT stencil_ref;
	ID3D11DepthStencilState *dss;
	ID3D11ShaderResourceView *ps_srv;
	ID3D11SamplerState *sampler;
	ID3D11PixelShader *ps;
	ID3D11VertexShader *vs;
	ID3D11GeometryShader *gs;
	UINT ps_count;
	UINT vs_count;
	UINT gs_count;
	ID3D11ClassInstance *ps_inst[256];
	ID3D11ClassInstance *vs_inst[256];
	ID3D11ClassInstance *gs_inst[256];
	D3D11_PRIMITIVE_TOPOLOGY topology;
	ID3D11Buffer *ib;
	ID3D11Buffer *vb;
	ID3D11Buffer *vs_cb;
	UINT ib_offset;
	UINT vb_stride;
	UINT vb_offset;
	DXGI_FORMAT ib_fmt;
	ID3D11InputLayout *il;
};

void *mty_d3d11_get_state(MTY_Device *device, MTY_Context *context)
{
	struct d3d11_state *s = MTY_Alloc(1, sizeof(struct d3d11_state));
	ID3D11DeviceContext *_context = (ID3D11DeviceContext *) context;

	s->sr_count = s->vp_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	ID3D11DeviceContext_RSGetScissorRects(_context, &s->sr_count, s->sr);
	ID3D11DeviceContext_RSGetViewports(_context, &s->vp_count, s->vp);
	ID3D11DeviceContext_RSGetState(_context, &s->rs);
	ID3D11DeviceContext_OMGetBlendState(_context, &s->bs, s->bf, &s->mask);
	ID3D11DeviceContext_OMGetDepthStencilState(_context, &s->dss, &s->stencil_ref);
	ID3D11DeviceContext_PSGetShaderResources(_context, 0, 1, &s->ps_srv);
	ID3D11DeviceContext_PSGetSamplers(_context, 0, 1, &s->sampler);

	s->ps_count = s->vs_count = s->gs_count = 256;
	ID3D11DeviceContext_PSGetShader(_context, &s->ps, s->ps_inst, &s->ps_count);
	ID3D11DeviceContext_VSGetShader(_context, &s->vs, s->vs_inst, &s->vs_count);
	ID3D11DeviceContext_VSGetConstantBuffers(_context, 0, 1, &s->vs_cb);
	ID3D11DeviceContext_GSGetShader(_context, &s->gs, s->gs_inst, &s->gs_count);

	ID3D11DeviceContext_IAGetPrimitiveTopology(_context, &s->topology);
	ID3D11DeviceContext_IAGetIndexBuffer(_context, &s->ib, &s->ib_fmt, &s->ib_offset);
	ID3D11DeviceContext_IAGetVertexBuffers(_context, 0, 1, &s->vb, &s->vb_stride, &s->vb_offset);
	ID3D11DeviceContext_IAGetInputLayout(_context, &s->il);

	return s;
}

void mty_d3d11_set_state(MTY_Device *device, MTY_Context *context, void *state)
{
	struct d3d11_state *s = state;
	ID3D11DeviceContext *_context = (ID3D11DeviceContext *) context;

	ID3D11DeviceContext_IASetInputLayout(_context, s->il);
	ID3D11DeviceContext_IASetVertexBuffers(_context, 0, 1, &s->vb, &s->vb_stride, &s->vb_offset);
	ID3D11DeviceContext_IASetIndexBuffer(_context, s->ib, s->ib_fmt, s->ib_offset);
	ID3D11DeviceContext_IASetPrimitiveTopology(_context, s->topology);

	ID3D11DeviceContext_GSSetShader(_context, s->gs, s->gs_inst, s->gs_count);
	ID3D11DeviceContext_VSSetConstantBuffers(_context, 0, 1, &s->vs_cb);
	ID3D11DeviceContext_VSSetShader(_context, s->vs, s->vs_inst, s->vs_count);
	ID3D11DeviceContext_PSSetShader(_context, s->ps, s->ps_inst, s->ps_count);

	ID3D11DeviceContext_PSSetSamplers(_context, 0, 1, &s->sampler);
	ID3D11DeviceContext_PSSetShaderResources(_context, 0, 1, &s->ps_srv);
	ID3D11DeviceContext_OMSetDepthStencilState(_context, s->dss, s->stencil_ref);
	ID3D11DeviceContext_OMSetBlendState(_context, s->bs, s->bf, s->mask);
	ID3D11DeviceContext_RSSetState(_context, s->rs);
	ID3D11DeviceContext_RSSetViewports(_context, s->vp_count, s->vp);
	ID3D11DeviceContext_RSSetScissorRects(_context, s->sr_count, s->sr);
}

void mty_d3d11_free_state(void **state)
{
	if (!state || !*state)
		return;

	struct d3d11_state *s = *state;

	if (s->il)
		ID3D11InputLayout_Release(s->il);

	if (s->vb)
		ID3D11Buffer_Release(s->vb);

	if (s->ib)
		ID3D11Buffer_Release(s->ib);

	if (s->gs)
		ID3D11GeometryShader_Release(s->gs);

	if (s->vs_cb)
		ID3D11Buffer_Release(s->vs_cb);

	for (UINT i = 0; i < s->vs_count; i++)
		if (s->vs_inst[i])
			ID3D11ClassInstance_Release(s->vs_inst[i]);

	if (s->vs)
		ID3D11VertexShader_Release(s->vs);

	for (UINT i = 0; i < s->ps_count; i++)
		if (s->ps_inst[i])
			ID3D11ClassInstance_Release(s->ps_inst[i]);

	if (s->ps)
		ID3D11PixelShader_Release(s->ps);

	if (s->sampler)
		ID3D11SamplerState_Release(s->sampler);

	if (s->ps_srv)
		ID3D11ShaderResourceView_Release(s->ps_srv);

	if (s->dss)
		ID3D11DepthStencilState_Release(s->dss);

	if (s->bs)
		ID3D11BlendState_Release(s->bs);

	if (s->rs)
		ID3D11RasterizerState_Release(s->rs);

	MTY_Free(s);
	*state = NULL;
}
