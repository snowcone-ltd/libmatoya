// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ui.h"
GFX_UI_PROTOTYPES(_metal_)

#include <math.h>

#import <Metal/Metal.h>

#include "shaders/metal/ui.h"

struct metal_ui {
	id<MTLRenderPipelineState> rps;
	id<MTLTexture> niltex;
	id<MTLBuffer> vb;
	id<MTLBuffer> ib;
	uint32_t vb_len;
	uint32_t ib_len;
};

struct gfx_ui *mty_metal_ui_create(MTY_Device *device)
{
	struct metal_ui *ctx = MTY_Alloc(1, sizeof(struct metal_ui));
	id<MTLDevice> _device = (__bridge id<MTLDevice>) device;

	bool r = true;
	NSError *error = nil;

	MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
	MTLRenderPipelineDescriptor *pdesc = [MTLRenderPipelineDescriptor new];
	MTLTextureDescriptor *tdesc = nil;

	id<MTLLibrary> library = [_device newLibraryWithSource:[NSString stringWithUTF8String:MTL_LIBRARY] options:nil error:&error];
	if (error) {
		r = false;
		MTY_Log("%s", [[error localizedDescription] UTF8String]);
		goto except;
	}

	// Pipeline description
	vertexDescriptor.attributes[0].offset = offsetof(MTY_Vtx, pos);
	vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
	vertexDescriptor.attributes[1].offset = offsetof(MTY_Vtx, uv);
	vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
	vertexDescriptor.attributes[2].offset = offsetof(MTY_Vtx, col);
	vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4;
	vertexDescriptor.layouts[0].stepRate = 1;
	vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
	vertexDescriptor.layouts[0].stride = sizeof(MTY_Vtx);

	pdesc.vertexFunction = [library newFunctionWithName:@"vs"];
	pdesc.fragmentFunction = [library newFunctionWithName:@"fs"];
	pdesc.vertexDescriptor = vertexDescriptor;
	pdesc.sampleCount = 1;
	pdesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	pdesc.colorAttachments[0].blendingEnabled = YES;
	pdesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
	pdesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
	pdesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	pdesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
	pdesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	pdesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

	ctx->rps = [_device newRenderPipelineStateWithDescriptor:pdesc error:&error];
	if (error) {
		r = false;
		MTY_Log("%s", [[error localizedDescription] UTF8String]);
		goto except;
	}

	// nil texture
	tdesc = [MTLTextureDescriptor new];
	tdesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
	tdesc.storageMode = MTLStorageModePrivate;
	tdesc.width = 64;
	tdesc.height = 64;
	ctx->niltex = [_device newTextureWithDescriptor:tdesc];

	except:

	if (!r)
		mty_metal_ui_destroy((struct gfx_ui **) &ctx, device);

	return (struct gfx_ui *) ctx;
}

bool mty_metal_ui_render(struct gfx_ui *gfx_ui, MTY_Device *device, MTY_Context *context,
	const MTY_DrawData *dd, MTY_Hash *cache, MTY_Surface *dest)
{
	struct metal_ui *ctx = (struct metal_ui *) gfx_ui;
	id<MTLDevice> _device = (__bridge id<MTLDevice>) device;
	id<MTLCommandQueue> cq = (__bridge id<MTLCommandQueue>) context;
	id<MTLTexture> _dest = (__bridge id<MTLTexture>) dest;

	// Prevent rendering under invalid scenarios
	if (dd->displaySize.x <= 0 || dd->displaySize.y <= 0 || dd->cmdListLength == 0)
		return false;

	// Resize vertex and index buffers if necessary
	if (dd->vtxTotalLength > ctx->vb_len) {
		ctx->vb_len = dd->vtxTotalLength + GFX_UI_VTX_INCR;
		ctx->vb = [_device newBufferWithLength:ctx->vb_len * sizeof(MTY_Vtx) options:MTLResourceCPUCacheModeWriteCombined];
	}

	if (dd->idxTotalLength > ctx->ib_len) {
		ctx->ib_len = dd->idxTotalLength + GFX_UI_IDX_INCR;
		ctx->ib = [_device newBufferWithLength:ctx->ib_len * sizeof(uint16_t) options:MTLResourceCPUCacheModeWriteCombined];
	}

	// Update the vertex shader's projection data based on the current display size
	float L = 0;
	float R = dd->displaySize.x;
	float T = 0;
	float B = dd->displaySize.y;
	const float proj[4][4] = {
		{2.0f / (R-L),   0.0f,         0.0f,  0.0f},
		{0.0f,           2.0f / (T-B), 0.0f,  0.0f},
		{0.0f,           0.0f,         1.0f,  0.0f},
		{(R+L) / (L-R), (T+B) / (B-T), 0.0f,  1.0f},
	};

	// Set viewport based on display size
	MTLViewport viewport = {0};
	viewport.width = dd->displaySize.x;
	viewport.height = dd->displaySize.y;
	viewport.zfar = 1.0;

	// Begin render pass, pipeline has been created in advance
	id<MTLCommandBuffer> cb = [cq commandBuffer];
	MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
	rpd.colorAttachments[0].texture = _dest;
	rpd.colorAttachments[0].loadAction = dd->clear ? MTLLoadActionClear : MTLLoadActionLoad;
	rpd.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
	rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];
	[re setViewport:viewport];
	[re setVertexBytes:&proj length:sizeof(proj) atIndex:1];
	[re setRenderPipelineState:ctx->rps];
	[re setVertexBuffer:ctx->vb offset:0 atIndex:0];

	// Draw
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;

	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		const MTY_CmdList *cmdList = &dd->cmdList[x];

		// Copy vertex, index buffer data
		memcpy((uint8_t *) ctx->vb.contents + vertexOffset, cmdList->vtx, cmdList->vtxLength * sizeof(MTY_Vtx));
		memcpy((uint8_t *) ctx->ib.contents + indexOffset, cmdList->idx, cmdList->idxLength * sizeof(uint16_t));

		for (uint32_t y = 0; y < cmdList->cmdLength; y++) {
			const MTY_Cmd *pcmd = &cmdList->cmd[y];
			const MTY_Rect *c = &pcmd->clip;

			// Make sure the rect is actually in the viewport
			if (c->left < dd->displaySize.x && c->top < dd->displaySize.y && c->right >= 0 && c->bottom >= 0) {

				// Use the clip to apply scissor
				MTLScissorRect scissorRect = {
					.x = lrint(c->left),
					.y = lrint(c->top),
					.width = lrint(c->right - c->left),
					.height = lrint(c->bottom - c->top)
				};
				[re setScissorRect:scissorRect];

				// Optionally sample from a texture (fonts, images)
				void *lookup = !pcmd->texture ? NULL : MTY_HashGetInt(cache, pcmd->texture);
				id<MTLTexture> tex = lookup ? (__bridge id<MTLTexture>) lookup : ctx->niltex;
				[re setFragmentTexture:tex atIndex:0];

				// Draw indexed
				[re setVertexBufferOffset:(vertexOffset + pcmd->vtxOffset * sizeof(MTY_Vtx)) atIndex:0];
				[re drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:pcmd->elemCount
					indexType:MTLIndexTypeUInt16 indexBuffer:ctx->ib
					indexBufferOffset:indexOffset + pcmd->idxOffset * sizeof(uint16_t)];
			}
		}

		vertexOffset += cmdList->vtxLength * sizeof(MTY_Vtx);
		indexOffset += cmdList->idxLength * sizeof(uint16_t);
	}

	// End render pass
	[re endEncoding];
	[cb commit];

	return true;
}

void *mty_metal_ui_create_texture(struct gfx_ui *gfx_ui, MTY_Device *device, const void *rgba,
	uint32_t width, uint32_t height)
{
	id<MTLDevice> _device = (__bridge id<MTLDevice>) device;

	MTLTextureDescriptor *tdesc = [MTLTextureDescriptor new];
	tdesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
	tdesc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
	tdesc.width = width;
	tdesc.height = height;
	id<MTLTexture> texture = [_device newTextureWithDescriptor:tdesc];

	[texture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:rgba bytesPerRow:width * 4];

	return (void *) CFBridgingRetain(texture);
}

void mty_metal_ui_destroy_texture(struct gfx_ui *gfx_ui, void **texture, MTY_Device *device)
{
	if (!texture || !*texture)
		return;

	id<MTLTexture> mtexture = (id<MTLTexture>) CFBridgingRelease(*texture);
	mtexture = nil;

	*texture = NULL;
}

void mty_metal_ui_destroy(struct gfx_ui **gfx_ui, MTY_Device *device)
{
	if (!gfx_ui || !*gfx_ui)
		return;

	struct metal_ui *ctx = (struct metal_ui *) *gfx_ui;

	ctx->rps = nil;
	ctx->niltex = nil;
	ctx->vb = nil;
	ctx->ib = nil;

	MTY_Free(ctx);
	*gfx_ui = NULL;
}
