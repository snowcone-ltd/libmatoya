// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

struct display_link {
	CVDisplayLinkRef link;
	dispatch_semaphore_t semaphore;
	MTY_Atomic64 counter;
};

static CVReturn display_link_output(CVDisplayLinkRef displayLink, const CVTimeStamp *inNow,
	const CVTimeStamp *inOutputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut,
	void *displayLinkContext)
{
	struct display_link *ctx = displayLinkContext;

	dispatch_semaphore_signal(ctx->semaphore);

	MTY_Atomic64Add(&ctx->counter, 1);

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

static int64_t display_link_get_counter(struct display_link *ctx)
{
	return MTY_Atomic64Get(&ctx->counter);
}

static void display_link_delay(struct display_link *ctx, uint32_t interval)
{
	for (uint32_t x = 0; x < interval; x++) {
		if (!ctx->link) {
			ctx->semaphore = dispatch_semaphore_create(0);
			CVDisplayLinkCreateWithCGDisplay(CGMainDisplayID(), &ctx->link);
			CVDisplayLinkSetOutputCallback(ctx->link, display_link_output, ctx);
			CVDisplayLinkStart(ctx->link);
		}

		dispatch_semaphore_wait(ctx->semaphore, DISPATCH_TIME_FOREVER);
	}
}
