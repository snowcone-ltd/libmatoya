#include "matoya.h"

struct context {
	MTY_App *app;
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

	// Set up a render description for the PNG
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

	// Draw the quad
	MTY_WindowDrawQuad(ctx->app, 0, ctx->image, &desc);

	MTY_WindowPresent(ctx->app, 0);

	return !ctx->quit;
}

int main(int argc, char **argv)
{
	struct context ctx = {0};
	ctx.app = MTY_AppCreate(app_func, event_func, &ctx);
	if (!ctx.app)
		return 1;

	MTY_WindowCreate(ctx.app, "My Window", NULL, 0);
	MTY_WindowSetGFX(ctx.app, 0, MTY_GFX_GL, true);

	// Fetch a PNG from the internet
	void *png = NULL;
	size_t png_size = 0;
	uint16_t code = 0;
	if (MTY_HttpRequest("user-images.githubusercontent.com", 0, true, "GET",
		"/328897/112402607-36d00780-8ce3-11eb-9707-d11bc6c73c59.png",
		NULL, NULL, 0, 5000, &png, &png_size, &code))
	{
		// On success, decompress it into RGBA
		if (code == 200)
			ctx.image = MTY_DecompressImage(png, png_size, &ctx.image_w, &ctx.image_h);

		MTY_Free(png);
	}

	MTY_AppRun(ctx.app);
	MTY_AppDestroy(&ctx.app);

	MTY_Free(ctx.image);

	return 0;
}
