// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod.h"
GFX_PROTOTYPES(_d3d9_)

#define COBJMACROS
#include <d3d9.h>

#include "gfx/viewport.h"
#include "gfx/fmt-d3d9.h"
#include "gfx/fmt.h"

static
#include "shaders/d3d9/ps.h"

static
#include "shaders/d3d9/vs.h"

#define D3D9_NUM_STAGING 3

struct d3d9_res {
	D3DFORMAT format;
	IDirect3DTexture9 *texture;
	IDirect3DBaseTexture9 *base;
	uint32_t width;
	uint32_t height;
};

struct d3d9 {
	MTY_ColorFormat format;
	struct d3d9_res staging[D3D9_NUM_STAGING];
	IDirect3DPixelShader9 *ps;
	IDirect3DVertexShader9 *vs;
	IDirect3DVertexBuffer9 *vb;
	IDirect3DVertexDeclaration9 *vd;
	IDirect3DIndexBuffer9 *ib;
};

struct gfx *mty_d3d9_create(MTY_Device *device)
{
	struct d3d9 *ctx = MTY_Alloc(1, sizeof(struct d3d9));
	IDirect3DDevice9 *_device = (IDirect3DDevice9 *) device;

	HRESULT e = IDirect3DDevice9_CreateVertexShader(_device, (DWORD *) vs, &ctx->vs);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateVertexShader' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDirect3DDevice9_CreatePixelShader(_device, (DWORD *) ps, &ctx->ps);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreatePixelShader' failed with HRESULT 0x%X", e);
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
		 1.0f,  1.0f,  // texcoord3
	};

	e = IDirect3DDevice9_CreateVertexBuffer(_device, sizeof(vertex_data), 0,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DPOOL_DEFAULT, &ctx->vb, NULL);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateVertexBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	void *ptr = NULL;
	e = IDirect3DVertexBuffer9_Lock(ctx->vb, 0, 0, &ptr, D3DLOCK_DISCARD);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DVertexBuffer9_Lock' failed with HRESULT 0x%X", e);
		goto except;
	}

	memcpy(ptr, vertex_data, sizeof(vertex_data));

	e = IDirect3DVertexBuffer9_Unlock(ctx->vb);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DVertexBuffer9_Unlock' failed with HRESULT 0x%X", e);
		goto except;
	}

	//vertex declaration (input layout)
	D3DVERTEXELEMENT9 dec[] = {
		{0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};

	e = IDirect3DDevice9_CreateVertexDeclaration(_device, dec, &ctx->vd);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateVertexDeclaration' failed with HRESULT 0x%X", e);
		goto except;
	}

	//index buffer
	DWORD index_data[] = {
		0, 1, 2,
		2, 3, 0
	};

	e = IDirect3DDevice9_CreateIndexBuffer(_device, sizeof(index_data),
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ctx->ib, NULL);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateIndexBuffer' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDirect3DIndexBuffer9_Lock(ctx->ib, 0, 0, &ptr, D3DLOCK_DISCARD);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DIndexBuffer9_Lock' failed with HRESULT 0x%X", e);
		goto except;
	}

	memcpy(ptr, index_data, sizeof(index_data));

	e = IDirect3DIndexBuffer9_Unlock(ctx->ib);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DIndexBuffer9_Unlock' failed with HRESULT 0x%X", e);
		goto except;
	}

	except:

	if (e != D3D_OK)
		mty_d3d9_destroy((struct gfx **) &ctx, device);

	return (struct gfx *) ctx;
}

static void d3d9_destroy_resource(struct d3d9_res *res)
{
	if (res->base) {
		IDirect3DBaseTexture9_Release(res->base);
		res->base = NULL;
	}

	if (res->texture) {
		IDirect3DTexture9_Release(res->texture);
		res->texture = NULL;
	}

	memset(res, 0, sizeof(struct d3d9_res));
}

static bool d3d9_crop_copy(struct d3d9_res *res, const uint8_t *image, uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp)
{
	D3DLOCKED_RECT rect = {0};
	HRESULT e = IDirect3DTexture9_LockRect(res->texture, 0, &rect, NULL, D3DLOCK_DISCARD);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DTexture9_LockRect' failed with HRESULT 0x%X", e);
		return false;
	}

	for (uint32_t y = 0; y < h; y++)
		memcpy((uint8_t *) rect.pBits + (y * rect.Pitch), image + (y * full_w * bpp), w * bpp);

	e = IDirect3DTexture9_UnlockRect(res->texture, 0);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DTexture9_UnlockRect' failed with HRESULT 0x%X", e);
		return false;
	}

	return true;
}

static bool d3d9_refresh_resource(struct gfx *gfx, MTY_Device *_device, MTY_Context *context, MTY_ColorFormat fmt,
	uint8_t plane, const uint8_t *image, uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp)
{
	struct d3d9 *ctx = (struct d3d9 *) gfx;

	bool r = true;

	struct d3d9_res *res = &ctx->staging[plane];
	D3DFORMAT format = FMT_PLANES[fmt][plane];

	// Resize texture
	if (!res->texture || w != res->width || h != res->height || format != res->format) {
		IDirect3DDevice9 *device = (IDirect3DDevice9 *) _device;

		d3d9_destroy_resource(res);

		HRESULT e = IDirect3DDevice9_CreateTexture(device, w, h, 1, D3DUSAGE_DYNAMIC,
			format, D3DPOOL_DEFAULT, &res->texture, NULL);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DDevice9_CreateTexture' failed with HRESULT 0x%X", e);
			r = false;
			goto except;
		}

		e = IDirect3DTexture9_QueryInterface(res->texture, &IID_IDirect3DBaseTexture9,
			(void **) &res->base);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DTexture9_QueryInterface' failed with HRESULT 0x%X", e);
			r = false;
			goto except;
		}

		res->width = w;
		res->height = h;
		res->format = format;
	}

	except:

	if (r) {
		// Upload
		r = d3d9_crop_copy(res, image, full_w, w, h, bpp);

	} else {
		d3d9_destroy_resource(res);
	}

	return r;
}

bool mty_d3d9_render(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest)
{
	struct d3d9 *ctx = (struct d3d9 *) gfx;
	IDirect3DDevice9 *_device = (IDirect3DDevice9 *) device;
	IDirect3DSurface9 *_dest = (IDirect3DSurface9 *) dest;

	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	// Refresh textures and load texture data
	// If format == MTY_COLOR_FORMAT_UNKNOWN, texture refreshing/loading is skipped and the previous frame is rendered
	if (!fmt_reload_textures(gfx, device, (MTY_Context *) device, image, desc, d3d9_refresh_resource))
		return false;

	// Begin render pass
	HRESULT e = IDirect3DDevice9_SetRenderTarget(_device, 0, _dest);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetRenderTarget' failed with HRESULT 0x%X", e);
		return false;
	}

	e = IDirect3DDevice9_Clear(_device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 0), 0, 0);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_Clear' failed with HRESULT 0x%X", e);
		return false;
	}

	// Viewport
	float vpx, vpy, vpw, vph;
	mty_viewport(desc, &vpx, &vpy, &vpw, &vph);

	D3DVIEWPORT9 vp = {0};
	vp.X = lrint(vpx);
	vp.Y = lrint(vpy);
	vp.Width = lrint(vpw);
	vp.Height = lrint(vph);

	e = IDirect3DDevice9_SetViewport(_device, &vp);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetViewport' failed with HRESULT 0x%X", e);
		return false;
	}

	// Vertex shader
	e = IDirect3DDevice9_SetVertexShader(_device, ctx->vs);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetVertexShader' failed with HRESULT 0x%X", e);
		return false;
	}

	e = IDirect3DDevice9_SetStreamSource(_device, 0, ctx->vb, 0, 4 * sizeof(float));
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetStreamSource' failed with HRESULT 0x%X", e);
		return false;
	}

	e = IDirect3DDevice9_SetVertexDeclaration(_device, ctx->vd);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetVertexDeclaration' failed with HRESULT 0x%X", e);
		return false;
	}

	e = IDirect3DDevice9_SetIndices(_device, ctx->ib);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetIndicies' failed with HRESULT 0x%X", e);
		return false;
	}

	// D3D9 half texel fix
	float texel_offset[4] = {0};
	texel_offset[0] = -1.0f / (float) vp.Width;
	texel_offset[1] = 1.0f / (float) vp.Height;

	e = IDirect3DDevice9_SetVertexShaderConstantF(_device, 0, texel_offset, 1);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetVertexShaderConstantF' failed with HRESULT 0x%X", e);
		return false;
	}

	// Pixel shader
	e = IDirect3DDevice9_SetPixelShader(_device, ctx->ps);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetPixelShader' failed with HRESULT 0x%X", e);
		return false;
	}

	for (uint8_t x = 0; x < D3D9_NUM_STAGING; x++) {
		if (ctx->staging[x].base) {
			DWORD sampler = desc->filter == MTY_FILTER_NEAREST ? D3DTEXF_POINT : D3DTEXF_LINEAR;
			IDirect3DDevice9_SetTexture(_device, x, ctx->staging[x].base);

			e = IDirect3DDevice9_SetSamplerState(_device, x, D3DSAMP_MAGFILTER, sampler);
			if (e != D3D_OK) {
				MTY_Log("'IDirect3DDevice9_SetSamplerState' failed with HRESULT 0x%X", e);
				return false;
			}

			e = IDirect3DDevice9_SetSamplerState(_device, x, D3DSAMP_MINFILTER, sampler);
			if (e != D3D_OK) {
				MTY_Log("'IDirect3DDevice9_SetSamplerState' failed with HRESULT 0x%X", e);
				return false;
			}
		}
	}

	// Uniforms
	float cb[3][4] = {0};

	// cb0
	cb[0][0] = (float) desc->cropWidth;
	cb[0][1] = (float) desc->cropHeight;
	cb[0][2] = vph;

	// cb1
	cb[1][0] = desc->effects[0];
	cb[1][1] = desc->effects[1];
	cb[1][2] = desc->levels[0];
	cb[1][3] = desc->levels[1];

	// cb2
	cb[2][0] = FMT_INFO[ctx->format].planes;
	cb[2][1] = desc->rotation;
	cb[2][2] = (float) FMT_CONVERSION(ctx->format, desc->fullRangeYUV, desc->multiplyYUV);

	e = IDirect3DDevice9_SetPixelShaderConstantF(_device, 0, (float *) cb, 3);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetPixelShaderConstantF' failed with HRESULT 0x%X", e);
		return false;
	}

	// Draw
	e = IDirect3DDevice9_DrawIndexedPrimitive(_device, D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_DrawIndexedPrimitive' failed with HRESULT 0x%X", e);
		return false;
	}

	return true;
}

void mty_d3d9_clear(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	uint32_t width, uint32_t height, float r, float g, float b, float a, MTY_Surface *dest)
{
	IDirect3DDevice9 *_device = (IDirect3DDevice9 *) device;
	IDirect3DSurface9 *_dest = (IDirect3DSurface9 *) dest;

	HRESULT e = IDirect3DDevice9_SetRenderTarget(_device, 0, _dest);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetRenderTarget' failed with HRESULT 0x%X", e);
		return;
	}

	e = IDirect3DDevice9_Clear(_device, 0, NULL, D3DCLEAR_TARGET,
		D3DCOLOR_RGBA(lrint(r * 255), lrint(g * 255), lrint(b * 255), lrint(a * 255)), 0, 0);
	if (e != D3D_OK)
		MTY_Log("'IDirect3DDevice9_Clear' failed with HRESULT 0x%X", e);
}

void mty_d3d9_destroy(struct gfx **gfx, MTY_Device *device)
{
	if (!gfx || !*gfx)
		return;

	struct d3d9 *ctx = (struct d3d9 *) *gfx;

	for (uint8_t x = 0; x < D3D9_NUM_STAGING; x++)
		d3d9_destroy_resource(&ctx->staging[x]);

	if (ctx->ib)
		IDirect3DIndexBuffer9_Release(ctx->ib);

	if (ctx->vd)
		IDirect3DVertexDeclaration9_Release(ctx->vd);

	if (ctx->vb)
		IDirect3DVertexBuffer9_Release(ctx->vb);

	if (ctx->ps)
		IDirect3DPixelShader9_Release(ctx->ps);

	if (ctx->vs)
		IDirect3DVertexShader9_Release(ctx->vs);

	MTY_Free(ctx);
	*gfx = NULL;
}


// State

struct d3d9_state {
	IDirect3DStateBlock9 *block;
	D3DMATRIX world;
	D3DMATRIX view;
	D3DMATRIX proj;
};

void *mty_d3d9_get_state(MTY_Device *device, MTY_Context *context)
{
	struct d3d9_state *state = MTY_Alloc(1, sizeof(struct d3d9_state));

	IDirect3DDevice9 *_device = (IDirect3DDevice9 *) device;

	HRESULT e = IDirect3DDevice9_CreateStateBlock(_device, D3DSBT_ALL, &state->block);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateStateBlock' failed with HRESULT 0x%X", e);
		goto except;
	}

	IDirect3DDevice9_GetTransform(_device, D3DTS_WORLD, &state->world);
	IDirect3DDevice9_GetTransform(_device, D3DTS_VIEW, &state->view);
	IDirect3DDevice9_GetTransform(_device, D3DTS_PROJECTION, &state->proj);

	except:

	if (e != D3D_OK)
		mty_d3d9_free_state((void **) &state);

	return state;
}

void mty_d3d9_set_state(MTY_Device *device, MTY_Context *context, void *state)
{
	struct d3d9_state *s = state;
	IDirect3DDevice9 *_device = (IDirect3DDevice9 *) device;

	IDirect3DDevice9_SetTransform(_device, D3DTS_WORLD, &s->world);
	IDirect3DDevice9_SetTransform(_device, D3DTS_VIEW, &s->view);
	IDirect3DDevice9_SetTransform(_device, D3DTS_PROJECTION, &s->proj);

	IDirect3DStateBlock9_Apply(s->block);
}

void mty_d3d9_free_state(void **state)
{
	if (!state || !*state)
		return;

	struct d3d9_state *s = *state;

	IDirect3DStateBlock9_Release(s->block);

	MTY_Free(s);
	*state = NULL;
}
