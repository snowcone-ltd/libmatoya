// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "app.h"

#include <stdio.h>


// GFX

GFX_CTX_PROTOTYPES(_gl_)
GFX_CTX_PROTOTYPES(_d3d9_)
GFX_CTX_PROTOTYPES(_d3d11_)
GFX_CTX_PROTOTYPES(_d3d12_)
GFX_CTX_PROTOTYPES(_metal_)
GFX_CTX_DECLARE_TABLE()

MTY_Device *MTY_WindowGetDevice(MTY_App *app, MTY_Window window)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	return api != MTY_GFX_NONE ? GFX_CTX_API[api].get_device(gfx_ctx) : NULL;
}

MTY_Context *MTY_WindowGetContext(MTY_App *app, MTY_Window window)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	return api != MTY_GFX_NONE ? GFX_CTX_API[api].get_context(gfx_ctx) : NULL;
}

MTY_Surface *MTY_WindowGetSurface(MTY_App *app, MTY_Window window)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	return api != MTY_GFX_NONE ? GFX_CTX_API[api].get_surface(gfx_ctx) : NULL;
}

void MTY_WindowDrawQuad(MTY_App *app, MTY_Window window, const void *image, const MTY_RenderDesc *desc)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	if (api != MTY_GFX_NONE)
		GFX_CTX_API[api].draw_quad(gfx_ctx, image, desc);
}

void MTY_WindowDrawUI(MTY_App *app, MTY_Window window, const MTY_DrawData *dd)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	if (api != MTY_GFX_NONE)
		GFX_CTX_API[api].draw_ui(gfx_ctx, dd);
}

bool MTY_WindowHasUITexture(MTY_App *app, MTY_Window window, uint32_t id)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	return api != MTY_GFX_NONE && GFX_CTX_API[api].has_ui_texture(gfx_ctx, id);
}

bool MTY_WindowSetUITexture(MTY_App *app, MTY_Window window, uint32_t id, const void *rgba, uint32_t width, uint32_t height)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	return api != MTY_GFX_NONE && GFX_CTX_API[api].set_ui_texture(gfx_ctx, id, rgba, width, height);
}

bool MTY_WindowMakeCurrent(MTY_App *app, MTY_Window window, bool current)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	return api != MTY_GFX_NONE && GFX_CTX_API[api].make_current(gfx_ctx, current);
}

void MTY_WindowPresent(MTY_App *app, MTY_Window window, uint32_t numFrames)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX api = mty_window_get_gfx(app, window, &gfx_ctx);

	if (api != MTY_GFX_NONE)
		GFX_CTX_API[api].present(gfx_ctx, numFrames);
}

MTY_GFX MTY_WindowGetGFX(MTY_App *app, MTY_Window window)
{
	return mty_window_get_gfx(app, window, NULL);
}

bool MTY_WindowSetGFX(MTY_App *app, MTY_Window window, MTY_GFX api, bool vsync)
{
	struct gfx_ctx *gfx_ctx = NULL;
	MTY_GFX wapi = mty_window_get_gfx(app, window, &gfx_ctx);

	if (wapi != MTY_GFX_NONE) {
		GFX_CTX_API[wapi].destroy(&gfx_ctx);
		mty_window_set_gfx(app, window, MTY_GFX_NONE, NULL);
	}

	void *native = mty_window_get_native(app, window);

	if (native && api != MTY_GFX_NONE) {
		gfx_ctx = GFX_CTX_API[api].create(native, vsync);

		// Fallback
		if (!gfx_ctx) {
			if (api == MTY_GFX_D3D11)
				return MTY_WindowSetGFX(app, window, MTY_GFX_D3D9, vsync);

			if (api == MTY_GFX_D3D9 || api == MTY_GFX_METAL)
				return MTY_WindowSetGFX(app, window, MTY_GFX_GL, vsync);

		} else {
			mty_window_set_gfx(app, window, api, gfx_ctx);
		}
	}

	return gfx_ctx ? true : false;
}


// Event utility

void MTY_PrintEvent(const MTY_Event *evt)
{
	#define PFMT(name, evt, fmt, ...) \
		printf("[%d] %-21s" fmt "\n", evt->window, #name, ## __VA_ARGS__)

	#define PEVENT(name, evt, fmt, ...) \
		case name: PFMT(name, evt, fmt, ## __VA_ARGS__); break

	switch (evt->type) {
		PEVENT(MTY_EVENT_CLOSE, evt, "");
		PEVENT(MTY_EVENT_QUIT, evt, "");
		PEVENT(MTY_EVENT_SHUTDOWN, evt, "");
		PEVENT(MTY_EVENT_KEY, evt, "key: 0x%X, mod: 0x%X, pressed: %u", evt->key.key, evt->key.mod, evt->key.pressed);
		PEVENT(MTY_EVENT_HOTKEY, evt, "id: %u", evt->hotkey);
		PEVENT(MTY_EVENT_TEXT, evt, "text: %s", evt->text);
		PEVENT(MTY_EVENT_SCROLL, evt, "x: %d, y: %d, pixels: %u", evt->scroll.x, evt->scroll.y, evt->scroll.pixels);
		PEVENT(MTY_EVENT_FOCUS, evt, "focus: %u", evt->focus);
		PEVENT(MTY_EVENT_MOTION, evt, "x: %d, y: %d, relative: %u, synth: %u", evt->motion.x,
			evt->motion.y, evt->motion.relative, evt->motion.synth);
		PEVENT(MTY_EVENT_BUTTON, evt, "x: %d, y: %d, button: %u, pressed: %u", evt->button.x,
			evt->button.y, evt->button.button, evt->button.pressed);
		PEVENT(MTY_EVENT_DROP, evt, "name: %s, buf: %p, size: %zu", evt->drop.name, evt->drop.buf, evt->drop.size);
		PEVENT(MTY_EVENT_CLIPBOARD, evt, "");
		PEVENT(MTY_EVENT_TRAY, evt, "id: %u", evt->trayID);
		PEVENT(MTY_EVENT_REOPEN, evt, "arg: %s", evt->reopenArg);
		PEVENT(MTY_EVENT_PEN, evt, "x: %u, y: %u, flags: 0x%X, pressure: %u, rotation: %u, tiltX: %d, tiltY: %d",
			evt->pen.x, evt->pen.y, evt->pen.flags, evt->pen.pressure, evt->pen.rotation, evt->pen.tiltX, evt->pen.tiltY);
		PEVENT(MTY_EVENT_CONNECT, evt, "id: %u", evt->controller.id);
		PEVENT(MTY_EVENT_DISCONNECT, evt, "id: %u", evt->controller.id);
		PEVENT(MTY_EVENT_SIZE, evt, "");
		PEVENT(MTY_EVENT_MOVE, evt, "");
		PEVENT(MTY_EVENT_BACK, evt, "");

		case MTY_EVENT_CONTROLLER: {
			PFMT(MTY_EVENT_CONTROLLER, evt, "id: %u, type: %u, vid-pid: %04X-%04X, numButtons: %u, numAxes: %u",
				evt->controller.id, evt->controller.type, evt->controller.vid, evt->controller.pid,
				evt->controller.numButtons, evt->controller.numAxes);

			printf("  buttons: ");
			for (uint8_t x = 0; x < evt->controller.numButtons; x++)
				printf("%u", evt->controller.buttons[x]);
			printf("\n");

			printf("  axes: ");
			for (uint8_t x = 0; x < evt->controller.numAxes; x++) {
				const MTY_Axis *a = &evt->controller.axes[x];
				printf("[%X] %-7d", a->usage, a->value);
			}
			printf("\n");
		}
	}
}
