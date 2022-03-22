#include "matoya.h"

struct MTY_Zoom {
	bool scaling;

	MTY_Point image;
	MTY_Point image_min;
	MTY_Point image_max;
	MTY_Point focus;

	uint32_t image_w;
	uint32_t image_h;
	uint32_t window_w;
	uint32_t window_h;

	float scale_screen;
	float scale_screen_min;
	float scale_screen_max;
	float scale_image;
	float scale_image_min;
	float scale_image_max;
	float scale_delta;
};

MTY_Zoom *MTY_ZoomCreate(uint32_t windowWidth, uint32_t windowHeight) 
{
	MTY_Zoom *ctx = MTY_Alloc(1, sizeof(MTY_Zoom));

	ctx->scale_delta = 1;
	ctx->scale_screen = 1;
	ctx->scale_screen_min = 1;
	ctx->scale_screen_max = 4;
	ctx->scale_delta = 1;
	ctx->window_w = windowWidth;
	ctx->window_h = windowHeight;

	return ctx;
}

static void mty_zoom_reset(MTY_Zoom *ctx, uint32_t imageWidth, uint32_t imageHeight, bool force)
{
	if (!ctx)
		return;

	bool same_image = ctx->image_w == imageWidth && ctx->image_h == imageHeight;
	if (!force && same_image)
		return;

	ctx->image_w  = imageWidth;
	ctx->image_h  = imageHeight;

	ctx->scale_screen = 1;
	ctx->scale_image = ctx->window_w < ctx->window_h
		? (float)ctx->window_w / ctx->image_w
		: (float)ctx->window_h / ctx->image_h;
	ctx->scale_delta = 1;

	ctx->image.x = 0;
	ctx->image.y = 0;
	if (ctx->window_w > ctx->window_h) 
		ctx->image.x = (ctx->window_w - ctx->image_w * ctx->scale_image) / 2.0f;
	if (ctx->window_w < ctx->window_h)
		ctx->image.y = (ctx->window_h - ctx->image_h * ctx->scale_image) / 2.0f;

	ctx->image_min.x = ctx->image.x;
	ctx->image_min.y = ctx->image.y;
	ctx->image_max.x = ctx->window_w - ctx->image.x;
	ctx->image_max.y = ctx->window_h - ctx->image.y;

	ctx->scale_image_min = ctx->scale_image * ctx->scale_screen_min;
	ctx->scale_image_max = ctx->scale_image * ctx->scale_screen_max;

	ctx->focus.x = 0;
	ctx->focus.y = 0;
}

void MTY_ZoomProcess(MTY_Zoom *ctx, uint32_t imageWidth, uint32_t imageHeight)
{
	if (!ctx)
		return;

	mty_zoom_reset(ctx, imageWidth, imageHeight, false);

	ctx->scale_screen *= ctx->scale_delta;
	ctx->scale_image  *= ctx->scale_delta;
	
	if (ctx->scale_screen < ctx->scale_screen_min) {
		ctx->scale_screen = ctx->scale_screen_min;
		ctx->scale_image  = ctx->scale_image_min;
		ctx->scale_delta  = 1;
	}

	if (ctx->scale_screen > ctx->scale_screen_max) {
		ctx->scale_screen = ctx->scale_screen_max;
		ctx->scale_image  = ctx->scale_image_max;
		ctx->scale_delta  = 1;
	}

	ctx->image.x = ctx->focus.x - ctx->scale_delta * (ctx->focus.x - ctx->image.x);
	ctx->image.y = ctx->focus.y - ctx->scale_delta * (ctx->focus.y - ctx->image.y);

	float image_scaled_w = ctx->image_w * ctx->scale_image;
	float image_scaled_h = ctx->image_h * ctx->scale_image;

	if (ctx->image.x > ctx->image_min.x)
		ctx->image.x = ctx->image_min.x;

	if (ctx->image.y > ctx->image_min.y)
		ctx->image.y = ctx->image_min.y;

	if (ctx->image.x < ctx->image_max.x - image_scaled_w)
		ctx->image.x = ctx->image_max.x - image_scaled_w;

	if (ctx->image.y < ctx->image_max.y - image_scaled_h)
		ctx->image.y = ctx->image_max.y - image_scaled_h;

	ctx->scale_delta = 1;
}

void MTY_ZoomFeed(MTY_Zoom *ctx, float scaleFactor, float focusX, float focusY)
{
	if (!ctx)
		return;

	if (ctx->scaling) {
		ctx->scale_delta *= scaleFactor;
		ctx->image.x += focusX - ctx->focus.x;
		ctx->image.y += focusY - ctx->focus.y;
	}

	ctx->focus.x = focusX;
	ctx->focus.y = focusY;
}

void MTY_ZoomResizeWindow(MTY_Zoom *ctx, uint32_t windowWidth, uint32_t windowHeight)
{
	ctx->window_w = windowWidth;
	ctx->window_h = windowHeight;

	mty_zoom_reset(ctx, ctx->image_w, ctx->image_h, true);
}

void MTY_ZoomSetLimits(MTY_Zoom *ctx, float min, float max)
{
	ctx->scale_screen_min = min;
	ctx->scale_screen_max = max;
}

int32_t MTY_ZoomTranformX(MTY_Zoom *ctx, int32_t value)
{
	float offset_x = - ctx->image.x / ctx->scale_screen + ctx->image_min.x;
	float zoom_w   = ctx->window_w / ctx->scale_screen;
	float ratio_x  = (float) value / ctx->window_w;

	return (int32_t) (offset_x + zoom_w * ratio_x);
}

int32_t MTY_ZoomTranformY(MTY_Zoom *ctx, int32_t value)
{
	float offset_y = - ctx->image.y / ctx->scale_screen + ctx->image_min.y;
	float zoom_h   = ctx->window_h / ctx->scale_screen;
	float ratio_y  = (float) value / ctx->window_h;

	return (int32_t) (offset_y + zoom_h * ratio_y);
}

float MTY_ZoomGetScale(MTY_Zoom *ctx)
{
	return ctx->scale_image;
}

int32_t MTY_ZoomGetImageX(MTY_Zoom *ctx)
{
	return (int32_t) ctx->image.x;
}

int32_t MTY_ZoomGetImageY(MTY_Zoom *ctx)
{
	return (int32_t) ctx->image.y;
}

void MTY_ZoomSetScaling(MTY_Zoom *ctx, bool scaling)
{
	ctx->scaling = scaling;
}

bool MTY_ZoomIsScaling(MTY_Zoom *ctx)
{
	return ctx->scaling;
}

void MTY_ZoomDestroy(MTY_Zoom **ctx)
{
	if (!(*ctx))
		return;

	MTY_Free(*ctx);
	*ctx = NULL;
}
