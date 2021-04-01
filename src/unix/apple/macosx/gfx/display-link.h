// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

struct display_link {
	CVDisplayLinkRef link;
	dispatch_semaphore_t semaphore;
};

static CVReturn metal_ctx_display_link(CVDisplayLinkRef displayLink, const CVTimeStamp *inNow,
	const CVTimeStamp *inOutputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut,
	void *displayLinkContext)
{
	struct display_link *ctx = displayLinkContext;

	dispatch_semaphore_signal(ctx->semaphore);

	return 0;
}

static void display_link_destroy(struct display_link *ctx)
{
	if (ctx->link) {
		CVDisplayLinkStop(ctx->link);
		CVDisplayLinkRelease(ctx->link);
		ctx->link = NULL;
	}

	ctx->semaphore = nil;
}

static void display_link_delay(struct display_link *ctx, uint32_t interval)
{
	for (uint32_t x = 0; interval > 1 && x < interval; x++) {
		if (!ctx->link) {
			ctx->semaphore = dispatch_semaphore_create(0);
			CVDisplayLinkCreateWithCGDisplay(CGMainDisplayID(), &ctx->link);
			CVDisplayLinkSetOutputCallback(ctx->link, metal_ctx_display_link, ctx);
			CVDisplayLinkStart(ctx->link);
		}

		dispatch_semaphore_wait(ctx->semaphore, DISPATCH_TIME_FOREVER);
	}
}
