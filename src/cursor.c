#include "matoya.h"

struct MTY_Cursor {
    MTY_App *app;
	void *image;
	uint32_t width;
	uint32_t height;
	uint16_t hotX;
	uint16_t hotY;
	int32_t x;
	int32_t y;
	float scale;
};

MTY_Cursor *MTY_CursorCreate(MTY_App *app)
{
    MTY_Cursor *ctx = MTY_Alloc(1, sizeof(MTY_Cursor));

    ctx->app = app;

    return ctx;
}

void MTY_CursorSetHotspot(MTY_Cursor *ctx, uint16_t hotX, uint16_t hotY)
{
    ctx->hotX = hotX;
    ctx->hotY = hotY;
}

void MTY_CursorSetImage(MTY_Cursor *ctx, const void *data, size_t size)
{
	if (ctx->image)
		MTY_Free(ctx->image);

    ctx->image = MTY_DecompressImage(data, size, &ctx->width, &ctx->height);
}

void MTY_CursorMove(MTY_Cursor *ctx, int32_t x, int32_t y, float scale)
{
    ctx->scale = scale;
    ctx->x = x;
    ctx->y = y;
}

void MTY_CursorMoveFromZoom(MTY_Cursor *ctx, MTY_Zoom *zoom)
{
	float scale = MTY_ZoomGetScale(zoom);
	int32_t cursor_x = MTY_ZoomGetCursorX(zoom);
	int32_t cursor_y = MTY_ZoomGetCursorY(zoom);
	
	MTY_CursorMove(ctx, cursor_x, cursor_y, scale);
}

void MTY_CursorDraw(MTY_Cursor *ctx, MTY_Window window)
{
	if (!ctx->image)
		return;
	
    MTY_RenderDesc desc = {0};

	desc.format = MTY_COLOR_FORMAT_BGRA;
	desc.cropWidth = ctx->width;
	desc.cropHeight = ctx->height;
	desc.imageWidth = ctx->width;
	desc.imageHeight = ctx->width;
	desc.aspectRatio = (float) ctx->width / (float) ctx->height;
	desc.blend = true;

	desc.type = MTY_POSITION_FIXED;
	desc.scale = ctx->scale;
	desc.imageX = ctx->x - ctx->hotX;
	desc.imageY = ctx->y - ctx->hotY;

	MTY_WindowDrawQuad(ctx->app, window, ctx->image, &desc);
}

void MTY_CursorDestroy(MTY_Cursor **ctx)
{
    if (!(*ctx))
        return;

    if ((*ctx)->image)
        MTY_Free((*ctx)->image);

    MTY_Free(*ctx);
    *ctx = NULL;
}