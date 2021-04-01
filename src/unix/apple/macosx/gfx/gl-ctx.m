// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>

#include "display-link.h"

struct gl_ctx {
	NSWindow *window;
	NSOpenGLContext *gl;
	MTY_Renderer *renderer;
	uint32_t interval;
	uint32_t fb0;
	CGSize size;

	struct display_link dlink;
};

static CGSize gl_ctx_get_size(struct gl_ctx *ctx)
{
	CGSize size = ctx->window.contentView.frame.size;
	float scale = ctx->window.screen.backingScaleFactor;

	size.width *= scale;
	size.height *= scale;

	return size;
}

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	struct gl_ctx *ctx = MTY_Alloc(1, sizeof(struct gl_ctx));
	ctx->window = (__bridge NSWindow *) native_window;
	ctx->renderer = MTY_RendererCreate();
	ctx->interval = 1;

	NSOpenGLPixelFormatAttribute attrs[] = {
		NSOpenGLPFAColorSize, 24,
		NSOpenGLPFAAlphaSize, 8,
		NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFANoRecovery,
		NSOpenGLPFATripleBuffer,
		0
	};

	NSOpenGLPixelFormat *pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
	ctx->gl = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
	[ctx->gl setView:ctx->window.contentView];

	return (struct gfx_ctx *) ctx;
}

void mty_gl_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct gl_ctx *ctx = (struct gl_ctx *) *gfx_ctx;

	MTY_RendererDestroy(&ctx->renderer);

	if (ctx->gl)
		[ctx->gl clearDrawable];

	display_link_destroy(&ctx->dlink);

	ctx->gl = nil;
	ctx->window = nil;

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

MTY_Device *mty_gl_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	return NULL;
}

MTY_Context *mty_gl_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return (__bridge MTY_Context *) ctx->gl;
}

static void gl_ctx_update(struct gl_ctx *ctx)
{
	if ([NSThread isMainThread]) {
		[ctx->gl update];

	} else {
		dispatch_sync(dispatch_get_main_queue(), ^{
			[ctx->gl update];
		});
	}
}

static void gl_ctx_refresh(struct gl_ctx *ctx)
{
	CGSize size = gl_ctx_get_size(ctx);

	if (size.width != ctx->size.width || size.height != ctx->size.height) {
		gl_ctx_update(ctx);
		ctx->size = size;
	}
}

MTY_Surface *mty_gl_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	gl_ctx_refresh(ctx);

	return (MTY_Surface *) &ctx->fb0;
}

void mty_gl_ctx_present(struct gfx_ctx *gfx_ctx, uint32_t interval)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	display_link_delay(&ctx->dlink, interval);

	// For vsync (inveral == 0)
	interval = interval > 0 ? 1 : 0;
	if (interval != ctx->interval) {
		[ctx->gl setValues:(GLint *) &interval forParameter:NSOpenGLCPSwapInterval];
		ctx->interval = interval;
	}

	[ctx->gl flushBuffer];
	glFinish();
}

void mty_gl_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	mty_gl_ctx_get_surface(gfx_ctx);

	MTY_RenderDesc mutated = *desc;
	mutated.viewWidth = lrint(ctx->size.width);
	mutated.viewHeight = lrint(ctx->size.height);

	MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_GL, NULL, NULL, image, &mutated, (MTY_Surface *) &ctx->fb0);
}

void mty_gl_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	mty_gl_ctx_get_surface(gfx_ctx);

	MTY_RendererDrawUI(ctx->renderer, MTY_GFX_GL, NULL, NULL, dd, (MTY_Surface *) &ctx->fb0);
}

bool mty_gl_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_GL, NULL, NULL, id, rgba, width, height);
}

bool mty_gl_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	return MTY_RendererHasUITexture(ctx->renderer, id);
}

bool mty_gl_ctx_make_current(struct gfx_ctx *gfx_ctx, bool current)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	if (current) {
		[ctx->gl makeCurrentContext];
		gl_ctx_update(ctx);

	} else {
		[NSOpenGLContext clearCurrentContext];
	}

	return true;
}
