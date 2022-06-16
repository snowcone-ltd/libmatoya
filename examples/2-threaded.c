#include "matoya.h"

struct context {
	MTY_App *app;
	MTY_Thread *thread;
	void *image;
	uint32_t image_w;
	uint32_t image_h;
	bool quit;
};

static void event_func(const MTY_Event *evt, void *opaque)
{
	struct context *ctx = opaque;

	MTY_PrintEvent(evt);

	if (evt->type == MTY_EVENT_CLOSE)
		ctx->quit = true;
}

static bool app_func(void *opaque)
{
	struct context *ctx = opaque;

	return !ctx->quit;
}

static void *render_thread(void *opaque)
{
	struct context *ctx = opaque;

	MTY_WindowSetGFX(ctx->app, 0, MTY_GFX_GL, true);
	MTY_WindowMakeCurrent(ctx->app, 0, true);

	while (!ctx->quit) {
		MTY_RenderDesc desc = {
			.format = MTY_COLOR_FORMAT_RGBA,
			.filter = MTY_FILTER_LINEAR,
			.effects = {MTY_EFFECT_SCANLINES},
			.levels = {0.85f},
			.imageWidth = ctx->image_w,
			.imageHeight = ctx->image_h,
			.cropWidth = ctx->image_w,
			.cropHeight = ctx->image_h,
		};

		MTY_WindowDrawQuad(ctx->app, 0, ctx->image, &desc);
		MTY_WindowPresent(ctx->app, 0, 1);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	struct context ctx = {0};
	ctx.app = MTY_AppCreate(app_func, event_func, &ctx);
	if (!ctx.app)
		return 1;

	MTY_WindowCreate(ctx.app, "My Window", NULL, false, false, 0);

	void *png = NULL;
	size_t png_size = 0;
	uint16_t code = 0;
	if (MTY_HttpRequest("user-images.githubusercontent.com", 0, true, "GET",
		"/328897/112402607-36d00780-8ce3-11eb-9707-d11bc6c73c59.png",
		NULL, NULL, 0, 5000, &png, &png_size, &code))
	{
		if (code == 200)
			ctx.image = MTY_DecompressImage(png, png_size, &ctx.image_w, &ctx.image_h);

		MTY_Free(png);
	}

	// Spawn a render thread that won't block the main thread
	ctx.thread = MTY_ThreadCreate(render_thread, &ctx);

	// app_func will now be called 500 times per second
	MTY_AppSetTimeout(ctx.app, 2);

	MTY_AppRun(ctx.app);

	// Clean up the render thread before destroying the app and windows
	MTY_ThreadDestroy(&ctx.thread);

	MTY_AppDestroy(&ctx.app);
	MTY_Free(ctx.image);

	return 0;
}
