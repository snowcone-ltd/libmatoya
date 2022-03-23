#include "matoya.h"

struct MTY_Zoom {
	bool scaling;
	bool relative;

	MTY_Point image;
	MTY_Point image_min;
	MTY_Point image_max;
	MTY_Point focus;

	MTY_Point origin;
	MTY_Point cursor;
	float margin;

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

MTY_Zoom *MTY_ZoomCreate() 
{
	MTY_Zoom *ctx = MTY_Alloc(1, sizeof(MTY_Zoom));

	ctx->scale_delta = 1;
	ctx->scale_screen = 1;
	ctx->scale_screen_min = 1;
	ctx->scale_screen_max = 4;
	ctx->scale_delta = 1;

	return ctx;
}

static void mty_zoom_reset(MTY_Zoom *ctx, uint32_t windowWidth, uint32_t windowHeight, uint32_t imageWidth, uint32_t imageHeight)
{
	if (!ctx)
		return;

	bool same_window = ctx->window_w == windowWidth && ctx->window_h == windowHeight;
	bool same_image  = ctx->image_w  == imageWidth  && ctx->image_h  == imageHeight;
	if (same_window && same_image)
		return;

	ctx->window_w = windowWidth;
	ctx->window_h = windowHeight;
	ctx->image_w = imageWidth;
	ctx->image_h = imageHeight;

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

	ctx->cursor.x = ctx->window_w / 2.0f;
	ctx->cursor.y = ctx->window_h / 2.0f;

	ctx->margin = (ctx->window_w < ctx->window_h ? ctx->window_w : ctx->window_h) * 0.2f;

	ctx->focus.x = 0;
	ctx->focus.y = 0;
}

static float mty_zoom_tranform_x(MTY_Zoom *ctx, float value)
{
	float offset_x = - ctx->image.x / ctx->scale_screen + ctx->image_min.x;
	float zoom_w   = ctx->window_w / ctx->scale_screen;
	float ratio_x  = (float) value / ctx->window_w;

	return offset_x + zoom_w * ratio_x;
}

static float mty_zoom_tranform_y(MTY_Zoom *ctx, float value)
{
	float offset_y = - ctx->image.y / ctx->scale_screen + ctx->image_min.y;
	float zoom_h   = ctx->window_h / ctx->scale_screen;
	float ratio_y  = (float) value / ctx->window_h;

	return offset_y + zoom_h * ratio_y;
}

void MTY_ZoomProcess(MTY_Zoom *ctx, uint32_t windowWidth, uint32_t windowHeight, uint32_t imageWidth, uint32_t imageHeight)
{
	if (!ctx)
		return;

	mty_zoom_reset(ctx, windowWidth, windowHeight, imageWidth, imageHeight);

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

	if (ctx->scaling) {
		ctx->cursor.x = mty_zoom_tranform_x(ctx, ctx->window_w / 2.0f);
		ctx->cursor.y = mty_zoom_tranform_y(ctx, ctx->window_h / 2.0f);
	}

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

void MTY_ZoomMove(MTY_Zoom *ctx, int32_t x, int32_t y, bool start)
{
	if (ctx->scaling)
		return;

	if (!ctx->relative) {
		ctx->cursor.x = mty_zoom_tranform_x(ctx, (float) x);
		ctx->cursor.y = mty_zoom_tranform_y(ctx, (float) y);
		return;
	}

	if (start) {
		ctx->origin.x = (float) x;
		ctx->origin.y = (float) y;
		return;
	}

	float delta_x = x - ctx->origin.x;
	float delta_y = y - ctx->origin.y;

	ctx->cursor.x += delta_x / ctx->scale_screen;
	ctx->cursor.y += delta_y / ctx->scale_screen;

	if (ctx->cursor.x < 0)
		ctx->cursor.x = 0;

	if (ctx->cursor.y < 0)
		ctx->cursor.y = 0;

	if (ctx->cursor.x > ctx->window_w)
		ctx->cursor.x = (float) ctx->window_w;

	if (ctx->cursor.y > ctx->window_h)
		ctx->cursor.y = (float) ctx->window_h;

	float left   = mty_zoom_tranform_x(ctx, ctx->margin);
	float right  = mty_zoom_tranform_x(ctx, ctx->window_w - ctx->margin);
	float top    = mty_zoom_tranform_y(ctx, ctx->margin);
	float bottom = mty_zoom_tranform_y(ctx, ctx->window_h - ctx->margin);

	if (delta_x < 0 && ctx->cursor.x < left)
		ctx->image.x -= delta_x;

	if (delta_x > 0 && ctx->cursor.x > right)
		ctx->image.x -= delta_x;

	if (delta_y < 0 && ctx->cursor.y < top)
		ctx->image.y -= delta_y;

	if (delta_y > 0 && ctx->cursor.y > bottom)
		ctx->image.y -= delta_y;

	ctx->origin.x = (float) x;
	ctx->origin.y = (float) y;
}

int32_t MTY_ZoomTranformX(MTY_Zoom *ctx, int32_t value)
{
	if (ctx->relative)
		return (int32_t) ctx->cursor.x;

	return (int32_t) mty_zoom_tranform_x(ctx, (float) value);
}

int32_t MTY_ZoomTranformY(MTY_Zoom *ctx, int32_t value)
{
	if (ctx->relative)
		return (int32_t) ctx->cursor.y;

	return (int32_t) mty_zoom_tranform_y(ctx, (float) value);
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

int32_t MTY_ZoomGetCursorX(MTY_Zoom *ctx)
{
	float left  = mty_zoom_tranform_x(ctx, 0);
	float right = mty_zoom_tranform_x(ctx, (float) ctx->window_w);

	return (int32_t) (ctx->window_w * (ctx->cursor.x - left) / (right - left));
}

int32_t MTY_ZoomGetCursorY(MTY_Zoom *ctx)
{
	float top    = mty_zoom_tranform_y(ctx, 0);
	float bottom = mty_zoom_tranform_y(ctx, (float) ctx->window_h);

	return (int32_t) (ctx->window_h * (ctx->cursor.y - top) / (bottom - top));
}

void MTY_ZoomSetScaling(MTY_Zoom *ctx, bool scaling)
{
	ctx->scaling = scaling;
}

bool MTY_ZoomIsScaling(MTY_Zoom *ctx)
{
	return ctx->scaling;
}

void MTY_ZoomSetRelative(MTY_Zoom *ctx, bool relative)
{
	ctx->relative = relative;
}

bool MTY_ZoomIsRelative(MTY_Zoom *ctx)
{
	return ctx->relative;
}

void MTY_ZoomSetLimits(MTY_Zoom *ctx, float min, float max)
{
	ctx->scale_screen_min = min;
	ctx->scale_screen_max = max;
}

void MTY_ZoomDestroy(MTY_Zoom **ctx)
{
	if (!(*ctx))
		return;

	MTY_Free(*ctx);
	*ctx = NULL;
}