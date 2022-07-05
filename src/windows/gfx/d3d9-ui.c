// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ui.h"
GFX_UI_PROTOTYPES(_d3d9_)

#include <math.h>

#define COBJMACROS
#include <d3d9.h>

#define D3D9_UI_CUSTOMVERTEX \
	(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

struct d3d9_ui {
	IDirect3DVertexBuffer9 *vb;
	IDirect3DIndexBuffer9 *ib;
	uint32_t vb_len;
	uint32_t idx_len;
};

struct d3d9_ui_vtx {
	float pos[3];
	D3DCOLOR col;
	float uv[2];
};

struct gfx_ui *mty_d3d9_ui_create(MTY_Device *device)
{
	return MTY_Alloc(1, sizeof(struct d3d9_ui));
}

bool mty_d3d9_ui_render(struct gfx_ui *gfx_ui, MTY_Device *device, MTY_Context *context,
	const MTY_DrawData *dd, MTY_Hash *cache, MTY_Surface *dest)
{
	struct d3d9_ui *ctx = (struct d3d9_ui *) gfx_ui;
	IDirect3DDevice9 *_device = (IDirect3DDevice9 *) device;
	IDirect3DSurface9 *_dest = (IDirect3DSurface9 *) dest;

	// Prevent rendering under invalid scenarios
	if (dd->displaySize.x <= 0 || dd->displaySize.y <= 0 || dd->cmdListLength == 0)
		return false;

	// Resize vertex and index buffers if necessary
	if (ctx->vb_len < dd->vtxTotalLength) {
		if (ctx->vb) {
			IDirect3DVertexBuffer9_Release(ctx->vb);
			ctx->vb = NULL;
		}

		HRESULT e = IDirect3DDevice9_CreateVertexBuffer(_device, (dd->vtxTotalLength + GFX_UI_VTX_INCR) * sizeof(struct d3d9_ui_vtx),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3D9_UI_CUSTOMVERTEX, D3DPOOL_DEFAULT, &ctx->vb, NULL);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DDevice9_CreateVertexBuffer' failed with HRESULT 0x%X", e);
			return false;
		}

		ctx->vb_len = dd->vtxTotalLength + GFX_UI_VTX_INCR;
	}

	if (ctx->idx_len < dd->idxTotalLength) {
		if (ctx->ib) {
			IDirect3DIndexBuffer9_Release(ctx->ib);
			ctx->ib = NULL;
		}

		HRESULT e = IDirect3DDevice9_CreateIndexBuffer(_device, (dd->idxTotalLength + GFX_UI_IDX_INCR) * sizeof(uint16_t),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ctx->ib, NULL);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DDevice9_CreateIndexBuffer' failed with HRESULT 0x%X", e);
			return false;
		}

		ctx->idx_len = dd->idxTotalLength + GFX_UI_IDX_INCR;
	}

	// Lock both vertex and index buffers and bulk copy the data
	struct d3d9_ui_vtx *vtx_dst = NULL;
	HRESULT e = IDirect3DVertexBuffer9_Lock(ctx->vb, 0, (UINT) (dd->vtxTotalLength * sizeof(struct d3d9_ui_vtx)),
		(void **) &vtx_dst, D3DLOCK_DISCARD);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DVertexBuffer9_Lock' failed with HRESULT 0x%X", e);
		return false;
	}

	uint16_t *idx_dst = NULL;
	e = IDirect3DIndexBuffer9_Lock(ctx->ib, 0, (UINT) (dd->idxTotalLength * sizeof(uint16_t)),
		(void **) &idx_dst, D3DLOCK_DISCARD);
	if (e != D3D_OK) {
		IDirect3DVertexBuffer9_Unlock(ctx->vb);
		MTY_Log("'IDirect3DIndexBuffer9_Lock' failed with HRESULT 0x%X", e);
		return false;
	}

	for (uint32_t n = 0; n < dd->cmdListLength; n++) {
		MTY_CmdList *cmdList = &dd->cmdList[n];
		MTY_Vtx *vtx_src = cmdList->vtx;

		// Imgui generates colors in RGBA, d3d9 needs to swizzle while copying to vertex buffer
		for (uint32_t i = 0; i < cmdList->vtxLength; i++) {
			vtx_dst->pos[0] = vtx_src->pos.x;
			vtx_dst->pos[1] = vtx_src->pos.y;
			vtx_dst->pos[2] = 0.0f;
			vtx_dst->col = (vtx_src->col & 0xFF00FF00) |
				((vtx_src->col & 0xFF0000) >> 16) | ((vtx_src->col & 0xFF) << 16);
			vtx_dst->uv[0] = vtx_src->uv.x;
			vtx_dst->uv[1] = vtx_src->uv.y;
			vtx_dst++;
			vtx_src++;
		}

		memcpy(idx_dst, cmdList->idx, cmdList->idxLength * sizeof(uint16_t));
		idx_dst += cmdList->idxLength;
	}

	IDirect3DIndexBuffer9_Unlock(ctx->ib);
	IDirect3DVertexBuffer9_Unlock(ctx->vb);

	// Update projection data based on the current display size -- there are no shaders
	float L = 0.5f;
	float R = dd->displaySize.x + 0.5f;
	float T = 0.5f;
	float B = dd->displaySize.y + 0.5f;
	float proj[4][4] = {
		2.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 2.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f
	};

	proj[0][0] /= R - L;
	proj[1][1] /= T - B;
	proj[3][0] = (R + L) / (L - R);
	proj[3][1] = (T + B) / (B - T);

	D3DMATRIX mat_proj = {0};
	memcpy(mat_proj.m, proj, sizeof(proj));

	// Set render target
	if (_dest) {
		IDirect3DDevice9_SetRenderTarget(_device, 0, _dest);

		// Clear render target to black
		if (dd->clear)
			IDirect3DDevice9_Clear(_device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	}

	// Set viewport based on display size
	D3DVIEWPORT9 vp = {0};
	vp.Width = lrint(dd->displaySize.x);
	vp.Height = lrint(dd->displaySize.y);
	vp.MaxZ = 1.0f;
	IDirect3DDevice9_SetViewport(_device, &vp);

	// Set up rendering pipeline
	D3DMATRIX mat_identity = {{{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	}}};

	IDirect3DDevice9_SetPixelShader(_device, NULL);
	IDirect3DDevice9_SetVertexShader(_device, NULL);
	IDirect3DDevice9_SetStreamSource(_device, 0, ctx->vb, 0, sizeof(struct d3d9_ui_vtx));
	IDirect3DDevice9_SetIndices(_device, ctx->ib);
	IDirect3DDevice9_SetFVF(_device, D3D9_UI_CUSTOMVERTEX);
	IDirect3DDevice9_SetTransform(_device, D3DTS_WORLD, &mat_identity);
	IDirect3DDevice9_SetTransform(_device, D3DTS_VIEW, &mat_identity);
	IDirect3DDevice9_SetTransform(_device, D3DTS_PROJECTION, &mat_proj);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_CULLMODE, D3DCULL_NONE);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_LIGHTING, false);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_ZENABLE, false);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_ALPHABLENDENABLE, true);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_ALPHATESTENABLE, false);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_BLENDOP, D3DBLENDOP_ADD);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_SCISSORTESTENABLE, true);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	IDirect3DDevice9_SetRenderState(_device, D3DRS_FOGENABLE, false);
	IDirect3DDevice9_SetTextureStageState(_device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	IDirect3DDevice9_SetTextureStageState(_device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	IDirect3DDevice9_SetTextureStageState(_device, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	IDirect3DDevice9_SetTextureStageState(_device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	IDirect3DDevice9_SetTextureStageState(_device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	IDirect3DDevice9_SetTextureStageState(_device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	IDirect3DDevice9_SetSamplerState(_device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	IDirect3DDevice9_SetSamplerState(_device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	// Draw
	uint32_t vtxOffset = 0;
	uint32_t idxOffset = 0;

	for (uint32_t n = 0; n < dd->cmdListLength; n++) {
		const MTY_CmdList *cmdList = &dd->cmdList[n];

		for (uint32_t cmd_i = 0; cmd_i < cmdList->cmdLength; cmd_i++) {
			MTY_Cmd *pcmd = &cmdList->cmd[cmd_i];

			// Use the clip to apply scissor
			RECT r = {0};
			r.left = lrint(pcmd->clip.left);
			r.top = lrint(pcmd->clip.top);
			r.right = lrint(pcmd->clip.right);
			r.bottom = lrint(pcmd->clip.bottom);

			// Make sure the rect is actually in the viewport
			if (r.left < dd->displaySize.x && r.top < dd->displaySize.y && r.right >= 0.0f && r.bottom >= 0.0f) {
				IDirect3DDevice9_SetScissorRect(_device, &r);

				// Optionally sample from a texture (fonts, images)
				IDirect3DDevice9_SetTexture(_device, 0, !pcmd->texture ? NULL :
					MTY_HashGetInt(cache, pcmd->texture));

				// Draw indexed
				IDirect3DDevice9_DrawIndexedPrimitive(_device, D3DPT_TRIANGLELIST, pcmd->vtxOffset + vtxOffset, 0,
					cmdList->vtxLength, pcmd->idxOffset + idxOffset, pcmd->elemCount / 3);
			}
		}

		idxOffset += cmdList->idxLength;
		vtxOffset += cmdList->vtxLength;
	}

	return true;
}

void *mty_d3d9_ui_create_texture(struct gfx_ui *gfx_ui, MTY_Device *device, const void *rgba,
	uint32_t width, uint32_t height)
{
	IDirect3DDevice9 *_device = (IDirect3DDevice9 *) device;

	IDirect3DTexture9 *texture = NULL;
	D3DLOCKED_RECT rect = {0};

	HRESULT e = IDirect3DDevice9_CreateTexture(_device, width, height, 1, D3DUSAGE_DYNAMIC,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, NULL);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateTexture' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IDirect3DTexture9_LockRect(texture, 0, &rect, NULL, 0);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DTexture9_LockRect' failed with HRESULT 0x%X", e);
		goto except;
	}

	for (uint32_t h = 0; h < height; h++) {
		for (uint32_t w = 0; w < width * 4; w += 4) {
			uint8_t *dest = (uint8_t *) rect.pBits + (h * rect.Pitch);
			const uint8_t *src = (const uint8_t *) rgba + (h * width * 4);

			dest[w + 0] = src[w + 2];
			dest[w + 1] = src[w + 1];
			dest[w + 2] = src[w + 0];
			dest[w + 3] = src[w + 3];
		}
	}

	IDirect3DTexture9_UnlockRect(texture, 0);

	except:

	if (e != D3D_OK && texture) {
		IDirect3DTexture9_Release(texture);
		texture = NULL;
	}

	return texture;
}

void mty_d3d9_ui_destroy_texture(struct gfx_ui *gfx_ui, void **texture, MTY_Device *device)
{
	if (!texture || !*texture)
		return;

	IDirect3DTexture9 *texture9 = *texture;
	IDirect3DTexture9_Release(texture9);

	*texture = NULL;
}

void mty_d3d9_ui_destroy(struct gfx_ui **gfx_ui, MTY_Device *device)
{
	if (!gfx_ui || !*gfx_ui)
		return;

	struct d3d9_ui *ctx = (struct d3d9_ui *) *gfx_ui;

	if (ctx->vb)
		IDirect3DVertexBuffer9_Release(ctx->vb);

	if (ctx->ib)
		IDirect3DIndexBuffer9_Release(ctx->ib);

	MTY_Free(ctx);
	*gfx_ui = NULL;
}
