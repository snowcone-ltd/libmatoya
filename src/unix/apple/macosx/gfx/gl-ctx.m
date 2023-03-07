// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_gl_)

#include <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>

#include "scale.h"

struct gl_ctx {
	NSWindow *window;
	NSOpenGLContext *gl;
	uint32_t fb0;
	CGSize size;
};

static void gl_ctx_mt_block(void (^block)(void))
{
	if ([NSThread isMainThread]) {
		block();

	} else {
		dispatch_sync(dispatch_get_main_queue(), block);
	}
}

struct gfx_ctx *mty_gl_ctx_create(void *native_window, bool vsync)
{
	struct gl_ctx *ctx = MTY_Alloc(1, sizeof(struct gl_ctx));
	ctx->window = (__bridge NSWindow *) native_window;

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

	[ctx->gl makeCurrentContext];

	GLint interval = vsync ? 1 : 0;
	[ctx->gl setValues:&interval forParameter:NSOpenGLCPSwapInterval];

	gl_ctx_mt_block(^{
		[ctx->gl setView:ctx->window.contentView];
	});

	return (struct gfx_ctx *) ctx;
}

void mty_gl_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct gl_ctx *ctx = (struct gl_ctx *) *gfx_ctx;

	if (ctx->gl)
		[ctx->gl clearDrawable];

	ctx->gl = nil;
	ctx->window = nil;

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

void mty_gl_ctx_get_size(struct gfx_ctx *gfx_ctx, uint32_t *w, uint32_t *h)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	*w = lrint(ctx->size.width);
	*h = lrint(ctx->size.height);
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

static void gl_ctx_refresh(struct gl_ctx *ctx)
{
	CGSize size = ctx->window.contentView.frame.size;
	CGFloat scale = mty_screen_scale(ctx->window.screen);

	size.width *= scale;
	size.height *= scale;

	if (size.width != ctx->size.width || size.height != ctx->size.height) {
		gl_ctx_mt_block(^{
			[ctx->gl update];
		});

		ctx->size = size;
	}
}

MTY_Surface *mty_gl_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	gl_ctx_refresh(ctx);

	return (MTY_Surface *) &ctx->fb0;
}

void mty_gl_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct gl_ctx *ctx = (struct gl_ctx *) gfx_ctx;

	[ctx->gl flushBuffer];
	glFinish();
}

bool mty_gl_ctx_lock(struct gfx_ctx *gfx_ctx)
{
	return true;
}

void mty_gl_ctx_unlock(void)
{
}
