// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_metal_)

#include <Metal/Metal.h>
#include <AppKit/AppKit.h>
#include <QuartzCore/CAMetalLayer.h>

#include "scale.h"

struct metal_ctx {
	NSWindow *window;
	CAMetalLayer *layer;
	id<CAMetalDrawable> back_buffer;
	id<MTLCommandQueue> cq;
	MTY_Renderer *renderer;
	CGSize size;
};

static CGSize metal_ctx_get_size(struct metal_ctx *ctx)
{
	CGSize size = ctx->window.contentView.frame.size;
	CGFloat scale = mty_screen_scale(ctx->window.screen);

	size.width *= scale;
	size.height *= scale;

	return size;
}

static void metal_ctx_mt_block(void (^block)(void))
{
	if ([NSThread isMainThread]) {
		block();

	} else {
		dispatch_sync(dispatch_get_main_queue(), block);
	}
}

struct gfx_ctx *mty_metal_ctx_create(void *native_window, bool vsync)
{
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();

	if (!device || ![device supportsFeatureSet: MTLFeatureSet_macOS_GPUFamily1_v1])
		return NULL;

	struct metal_ctx *ctx = MTY_Alloc(1, sizeof(struct metal_ctx));
	ctx->window = (__bridge NSWindow *) native_window;
	ctx->renderer = MTY_RendererCreate();

	metal_ctx_mt_block(^{
		ctx->layer = [CAMetalLayer layer];
		ctx->layer.device = device;
		ctx->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		ctx->layer.displaySyncEnabled = vsync ? YES : NO;

		ctx->cq = [ctx->layer.device newCommandQueue];

		ctx->window.contentView.wantsLayer = YES;
		ctx->window.contentView.layer = ctx->layer;
	});

	return (struct gfx_ctx *) ctx;
}

void mty_metal_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct metal_ctx *ctx = (struct metal_ctx *) *gfx_ctx;

	MTY_RendererDestroy(&ctx->renderer);

	ctx->window = nil;
	ctx->layer = nil;
	ctx->cq = nil;
	ctx->back_buffer = nil;

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

MTY_Device *mty_metal_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	return (__bridge MTY_Device *) ctx->cq.device;
}

MTY_Context *mty_metal_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	return (__bridge MTY_Context *) ctx->cq;
}

static void metal_ctx_refresh(struct metal_ctx *ctx)
{
	CGSize size = metal_ctx_get_size(ctx);

	if (size.width != ctx->size.width || size.height != ctx->size.height) {
		ctx->layer.drawableSize = size;
		ctx->size = size;
	}
}

MTY_Surface *mty_metal_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	@autoreleasepool {
		if (!ctx->back_buffer) {
			metal_ctx_refresh(ctx);
			ctx->back_buffer = [ctx->layer nextDrawable];
		}
	}

	return (__bridge MTY_Surface *) ctx->back_buffer.texture;
}

void mty_metal_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	if (ctx->back_buffer) {
		id<MTLCommandBuffer> cb = [ctx->cq commandBuffer];
		[cb presentDrawable:ctx->back_buffer];
		[cb commit];
		[cb waitUntilCompleted];

		ctx->back_buffer = nil;
	}
}

void mty_metal_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	mty_metal_ctx_get_surface(gfx_ctx);

	if (ctx->back_buffer) {
		MTY_RenderDesc mutated = *desc;
		mutated.viewWidth = lrint(ctx->size.width);
		mutated.viewHeight = lrint(ctx->size.height);

		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_METAL, (__bridge MTY_Device *) ctx->cq.device,
			(__bridge MTY_Context *) ctx->cq, image, &mutated, (__bridge MTY_Surface *) ctx->back_buffer.texture);
	}
}

void mty_metal_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	mty_metal_ctx_get_surface(gfx_ctx);

	if (ctx->back_buffer)
		MTY_RendererDrawUI(ctx->renderer, MTY_GFX_METAL, (__bridge MTY_Device *) ctx->cq.device,
			(__bridge MTY_Context *) ctx->cq, dd, (__bridge MTY_Surface *) ctx->back_buffer.texture);
}

bool mty_metal_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	return MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_METAL, (__bridge MTY_Device *) ctx->cq.device,
		(__bridge MTY_Context *) ctx->cq, id, rgba, width, height);
}

bool mty_metal_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct metal_ctx *ctx = (struct metal_ctx *) gfx_ctx;

	return MTY_RendererHasUITexture(ctx->renderer, id);
}
