// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "web.h"
#include "keymap.h"
#include "hid/utils.h"

struct MTY_App {
	MTY_Hash *hotkey;
	MTY_Hash *deduper;
	MTY_EventFunc event_func;
	MTY_AppFunc app_func;
	MTY_DetachState detach;
	MTY_ControllerEvent cevt[4];
	void *opaque;

	MTY_GFX api;
	bool kb_grab;
	struct gfx_ctx *gfx_ctx;
};

static void __attribute__((constructor)) app_global_init(void)
{
	web_set_mem_funcs(MTY_Alloc, MTY_Free);

	// WASI will buffer stdout and stderr by default, disable it
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
}


// Window events

static void window_motion(MTY_App *ctx, bool relative, int32_t x, int32_t y)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_MOTION;
	evt.motion.relative = relative;
	evt.motion.x = x;
	evt.motion.y = y;

	ctx->event_func(&evt, ctx->opaque);
}

static void window_resize(MTY_App *ctx)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_SIZE;

	ctx->event_func(&evt, ctx->opaque);
}

static void window_move(MTY_App *ctx)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_MOVE;

	ctx->event_func(&evt, ctx->opaque);
}

static void window_button(MTY_App *ctx, bool pressed, int32_t button, int32_t x, int32_t y)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_BUTTON;
	evt.button.pressed = pressed;
	evt.button.button =
		button == 0 ? MTY_BUTTON_LEFT :
		button == 1 ? MTY_BUTTON_MIDDLE :
		button == 2 ? MTY_BUTTON_RIGHT :
		button == 3 ? MTY_BUTTON_X1 :
		button == 4 ? MTY_BUTTON_X2 :
		MTY_BUTTON_NONE;

	evt.button.x = x;
	evt.button.y = y;

	ctx->event_func(&evt, ctx->opaque);
}

static void window_scroll(MTY_App *ctx, int32_t x, int32_t y)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_SCROLL;
	evt.scroll.x = x;
	evt.scroll.y = -y;

	ctx->event_func(&evt, ctx->opaque);
}

static void app_kb_to_hotkey(MTY_App *app, MTY_Event *evt)
{
	MTY_Mod mod = evt->key.mod & 0xFF;
	uint32_t hotkey = (uint32_t) (uintptr_t) MTY_HashGetInt(app->hotkey, (mod << 16) | evt->key.key);

	if (hotkey != 0) {
		if (evt->key.pressed) {
			evt->type = MTY_EVENT_HOTKEY;
			evt->hotkey = hotkey;

		} else {
			evt->type = MTY_EVENT_NONE;
		}
	}
}

static bool window_allow_default(MTY_Mod mod, MTY_Key key)
{
	// The "allowed" browser hotkey list. Copy/Paste, Refresh, fullscreen, developer console, and tab switching

	return
		(((mod & MTY_MOD_CTRL) || (mod & MTY_MOD_WIN)) && key == MTY_KEY_V) ||
		(((mod & MTY_MOD_CTRL) || (mod & MTY_MOD_WIN)) && key == MTY_KEY_C) ||
		((mod & MTY_MOD_CTRL) && (mod & MTY_MOD_SHIFT) && key == MTY_KEY_I) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_R) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_F5) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_1) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_2) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_3) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_4) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_5) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_6) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_7) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_8) ||
		((mod & MTY_MOD_CTRL) && key == MTY_KEY_9) ||
		(key == MTY_KEY_F5) ||
		(key == MTY_KEY_F11) ||
		(key == MTY_KEY_F12);
}

static bool window_keyboard(MTY_App *ctx, bool pressed, MTY_Key key, const char *text, uint32_t mods)
{
	MTY_Event evt = {0};

	if (text) {
		evt.type = MTY_EVENT_TEXT;
		snprintf(evt.text, 8, "%s", text);

		ctx->event_func(&evt, ctx->opaque);
	}

	if (key != 0) {
		evt.type = MTY_EVENT_KEY;
		evt.key.key = key;
		evt.key.pressed = pressed;

		if (mods & 0x01) evt.key.mod |= MTY_MOD_LSHIFT;
		if (mods & 0x02) evt.key.mod |= MTY_MOD_LCTRL;
		if (mods & 0x04) evt.key.mod |= MTY_MOD_LALT;
		if (mods & 0x08) evt.key.mod |= MTY_MOD_LWIN;
		if (mods & 0x10) evt.key.mod |= MTY_MOD_CAPS;
		if (mods & 0x20) evt.key.mod |= MTY_MOD_NUM;

		app_kb_to_hotkey(ctx, &evt);
		ctx->event_func(&evt, ctx->opaque);

		return !window_allow_default(evt.key.mod, evt.key.key) || ctx->kb_grab;
	}

	return true;
}

static void window_focus(MTY_App *ctx, bool focus)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_FOCUS;
	evt.focus = focus;

	ctx->event_func(&evt, ctx->opaque);
}

static void window_drop(MTY_App *ctx, const char *name, const void *data, size_t size)
{
	MTY_Event evt = {0};
	evt.type = MTY_EVENT_DROP;
	evt.drop.name = name;
	evt.drop.buf = data;
	evt.drop.size = size;

	ctx->event_func(&evt, ctx->opaque);
}

static void window_controller(MTY_App *ctx, uint32_t id, uint32_t state, uint32_t buttons,
	float lx, float ly, float rx, float ry, float lt, float rt)
{
	#define TESTB(i) \
		((buttons & (i)) == (i))

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONTROLLER;

	MTY_ControllerEvent *c = &evt.controller;
	c->type = MTY_CTYPE_DEFAULT;
	c->numButtons = 16;
	c->numAxes = 7;
	c->vid = 0xCDD;
	c->pid = 0xCDD;
	c->id = id;

	c->buttons[MTY_CBUTTON_A] = TESTB(0x0001);
	c->buttons[MTY_CBUTTON_B] = TESTB(0x0002);
	c->buttons[MTY_CBUTTON_X] = TESTB(0x0004);
	c->buttons[MTY_CBUTTON_Y] = TESTB(0x0008);
	c->buttons[MTY_CBUTTON_LEFT_SHOULDER] = TESTB(0x0010);
	c->buttons[MTY_CBUTTON_RIGHT_SHOULDER] = TESTB(0x0020);
	c->buttons[MTY_CBUTTON_BACK] = TESTB(0x0100);
	c->buttons[MTY_CBUTTON_START] = TESTB(0x0200);
	c->buttons[MTY_CBUTTON_LEFT_THUMB] = TESTB(0x0400);
	c->buttons[MTY_CBUTTON_RIGHT_THUMB] = TESTB(0x0800);

	c->axes[MTY_CAXIS_THUMB_LX].value = lx < 0.0f ? lrint(lx * abs(INT16_MIN)) : lrint(lx * INT16_MAX);
	c->axes[MTY_CAXIS_THUMB_LX].usage = 0x30;
	c->axes[MTY_CAXIS_THUMB_LX].min = INT16_MIN;
	c->axes[MTY_CAXIS_THUMB_LX].max = INT16_MAX;

	c->axes[MTY_CAXIS_THUMB_LY].value = ly > 0.0f ? lrint(-ly * abs(INT16_MIN)) : lrint(-ly * INT16_MAX);
	c->axes[MTY_CAXIS_THUMB_LY].usage = 0x31;
	c->axes[MTY_CAXIS_THUMB_LY].min = INT16_MIN;
	c->axes[MTY_CAXIS_THUMB_LY].max = INT16_MAX;

	c->axes[MTY_CAXIS_THUMB_RX].value = rx < 0.0f ? lrint(rx * abs(INT16_MIN)) : lrint(rx * INT16_MAX);
	c->axes[MTY_CAXIS_THUMB_RX].usage = 0x32;
	c->axes[MTY_CAXIS_THUMB_RX].min = INT16_MIN;
	c->axes[MTY_CAXIS_THUMB_RX].max = INT16_MAX;

	c->axes[MTY_CAXIS_THUMB_RY].value = ry > 0.0f ? lrint(-ry * abs(INT16_MIN)) : lrint(-ry * INT16_MAX);
	c->axes[MTY_CAXIS_THUMB_RY].usage = 0x35;
	c->axes[MTY_CAXIS_THUMB_RY].min = INT16_MIN;
	c->axes[MTY_CAXIS_THUMB_RY].max = INT16_MAX;

	c->axes[MTY_CAXIS_TRIGGER_L].value = lrint(lt * UINT8_MAX);
	c->axes[MTY_CAXIS_TRIGGER_L].usage = 0x33;
	c->axes[MTY_CAXIS_TRIGGER_L].min = 0;
	c->axes[MTY_CAXIS_TRIGGER_L].max = UINT8_MAX;

	c->axes[MTY_CAXIS_TRIGGER_R].value = lrint(rt * UINT8_MAX);
	c->axes[MTY_CAXIS_TRIGGER_R].usage = 0x34;
	c->axes[MTY_CAXIS_TRIGGER_R].min = 0;
	c->axes[MTY_CAXIS_TRIGGER_R].max = UINT8_MAX;

	c->buttons[MTY_CBUTTON_LEFT_TRIGGER] = c->axes[MTY_CAXIS_TRIGGER_L].value > 0;
	c->buttons[MTY_CBUTTON_RIGHT_TRIGGER] = c->axes[MTY_CAXIS_TRIGGER_R].value > 0;

	bool up = TESTB(0x1000);
	bool down = TESTB(0x2000);
	bool left = TESTB(0x4000);
	bool right = TESTB(0x8000);

	c->axes[MTY_CAXIS_DPAD].value = (up && right) ? 1 : (right && down) ? 3 :
		(down && left) ? 5 : (left && up) ? 7 : up ? 0 : right ? 2 : down ? 4 : left ? 6 : 8;
	c->axes[MTY_CAXIS_DPAD].usage = 0x39;
	c->axes[MTY_CAXIS_DPAD].min = 0;
	c->axes[MTY_CAXIS_DPAD].max = 7;

	// Connect
	if (state == 1) {
		MTY_Event cevt = evt;
		cevt.type = MTY_EVENT_CONNECT;
		ctx->event_func(&cevt, ctx->opaque);

	// Disconnect
	} else if (state == 2) {
		evt.type = MTY_EVENT_DISCONNECT;
	}

	// Dedupe and fire event
	if (mty_hid_dedupe(ctx->deduper, c) || evt.type != MTY_EVENT_CONTROLLER)
		ctx->event_func(&evt, ctx->opaque);
}


// App / Window

MTY_App *MTY_AppCreate(MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
{
	MTY_App *ctx = MTY_Alloc(1, sizeof(MTY_App));
	ctx->app_func = appFunc;
	ctx->event_func = eventFunc;
	ctx->opaque = opaque;

	web_attach_events(ctx, window_motion, window_button,
		window_scroll, window_keyboard, window_focus, window_drop,
		window_resize);

	ctx->hotkey = MTY_HashCreate(0);
	ctx->deduper = MTY_HashCreate(0);

	keymap_set_keys();

	return ctx;
}

void MTY_AppDestroy(MTY_App **app)
{
	if (!app || !*app)
		return;

	MTY_App *ctx = *app;

	MTY_HashDestroy(&ctx->hotkey, NULL);
	MTY_HashDestroy(&ctx->deduper, MTY_Free);

	MTY_Free(ctx);
	*app = NULL;
}

void MTY_AppRun(MTY_App *ctx)
{
	web_raf(ctx, ctx->app_func, window_controller, window_move, ctx->opaque);
}

void MTY_AppSetTimeout(MTY_App *ctx, uint32_t timeout)
{
}

bool MTY_AppIsActive(MTY_App *ctx)
{
	return web_has_focus();
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
}

void MTY_AppSetTray(MTY_App *ctx, const char *tooltip, const MTY_MenuItem *items, uint32_t len)
{
}

void MTY_AppRemoveTray(MTY_App *ctx)
{
}

void MTY_AppSendNotification(MTY_App *ctx, const char *title, const char *msg)
{
}

char *MTY_AppGetClipboard(MTY_App *ctx)
{
	return web_get_clipboard();
}

void MTY_AppSetClipboard(MTY_App *ctx, const char *text)
{
	web_set_clipboard(text);
}

void MTY_AppStayAwake(MTY_App *ctx, bool enable)
{
	web_wake_lock(enable);
}

MTY_DetachState MTY_AppGetDetachState(MTY_App *ctx)
{
	return ctx->detach;
}

void MTY_AppSetDetachState(MTY_App *ctx, MTY_DetachState state)
{
	ctx->detach = state;
}

bool MTY_AppIsMouseGrabbed(MTY_App *ctx)
{
	return false;
}

void MTY_AppGrabMouse(MTY_App *ctx, bool grab)
{
}

bool MTY_AppGetRelativeMouse(MTY_App *ctx)
{
	return web_get_relative();
}

void MTY_AppSetRelativeMouse(MTY_App *ctx, bool relative)
{
	web_set_pointer_lock(relative);
}

void MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
	web_set_png_cursor(image, size, hotX, hotY);
}

void MTY_AppUseDefaultCursor(MTY_App *ctx, bool useDefault)
{
	web_use_default_cursor(useDefault);
}

void MTY_AppShowCursor(MTY_App *ctx, bool show)
{
	web_show_cursor(show);
}

bool MTY_AppCanWarpCursor(MTY_App *ctx)
{
	return false;
}

bool MTY_AppIsKeyboardGrabbed(MTY_App *ctx)
{
	return ctx->kb_grab;
}

void MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
	ctx->kb_grab = grab;
}

uint32_t MTY_AppGetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key)
{
	mod &= 0xFF;

	return (uint32_t) (uintptr_t) MTY_HashGetInt(ctx->hotkey, (mod << 16) | key);
}

void MTY_AppSetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key, uint32_t id)
{
	mod &= 0xFF;
	MTY_HashSetInt(ctx->hotkey, (mod << 16) | key, (void *) (uintptr_t) id);
}

void MTY_AppRemoveHotkeys(MTY_App *ctx, MTY_Scope scope)
{
	MTY_HashDestroy(&ctx->hotkey, NULL);
	ctx->hotkey = MTY_HashCreate(0);
}

void MTY_AppEnableGlobalHotkeys(MTY_App *ctx, bool enable)
{
}

bool MTY_AppIsSoftKeyboardShowing(MTY_App *ctx)
{
	return false;
}

void MTY_AppShowSoftKeyboard(MTY_App *ctx, bool show)
{
}

MTY_Orientation MTY_AppGetOrientation(MTY_App *ctx)
{
	return MTY_ORIENTATION_USER;
}

void MTY_AppSetOrientation(MTY_App *ctx, MTY_Orientation orientation)
{
}

void MTY_AppRumbleController(MTY_App *ctx, uint32_t id, uint16_t low, uint16_t high)
{
	web_rumble_gamepad(id, (float) low / (float) UINT16_MAX, (float) high / (float) UINT16_MAX);
}

const void *MTY_AppGetControllerExtraData(MTY_App *ctx, uint32_t id, MTY_ExtraData type, size_t *size)
{
	return NULL;
}

bool MTY_AppIsPenEnabled(MTY_App *ctx)
{
	return false;
}

void MTY_AppEnablePen(MTY_App *ctx, bool enable)
{
}

MTY_InputMode MTY_AppGetInputMode(MTY_App *ctx)
{
	return MTY_INPUT_MODE_UNSPECIFIED;
}

void MTY_AppSetInputMode(MTY_App *ctx, MTY_InputMode mode)
{
}


// Window

MTY_Window MTY_WindowCreate(MTY_App *app, const MTY_WindowDesc *desc)
{
	MTY_WindowSetTitle(app, 0, desc->title ? desc->title : "MTY_Window");

	return 0;
}

void MTY_WindowDestroy(MTY_App *app, MTY_Window window)
{
}

bool MTY_WindowGetSize(MTY_App *app, MTY_Window window, uint32_t *width, uint32_t *height)
{
	web_get_size(width, height);

	return true;
}

void MTY_WindowGetPosition(MTY_App *app, MTY_Window window, int32_t *x, int32_t *y)
{
	web_get_position(x, y);
}

bool MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window, uint32_t *width, uint32_t *height)
{
	web_get_screen_size(width, height);

	return true;
}

float MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window)
{
	return web_get_pixel_ratio();
}

void MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title)
{
	web_set_title(title);
}

bool MTY_WindowIsVisible(MTY_App *app, MTY_Window window)
{
	return web_is_visible();
}

bool MTY_WindowIsActive(MTY_App *app, MTY_Window window)
{
	return web_has_focus();
}

void MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active)
{
}

bool MTY_WindowExists(MTY_App *app, MTY_Window window)
{
	return true;
}

bool MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window)
{
	return web_get_fullscreen();
}

void MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen)
{
	web_set_fullscreen(fullscreen);
}

void MTY_WindowWarpCursor(MTY_App *app, MTY_Window window, uint32_t x, uint32_t y)
{
}

MTY_ContextState MTY_WindowGetContextState(MTY_App *app, MTY_Window window)
{
	return MTY_CONTEXT_STATE_NORMAL;
}


// Window Private

void mty_window_set_gfx(MTY_App *app, MTY_Window window, MTY_GFX api, struct gfx_ctx *gfx_ctx)
{
	app->api = api;
	app->gfx_ctx = gfx_ctx;
}

MTY_GFX mty_window_get_gfx(MTY_App *app, MTY_Window window, struct gfx_ctx **gfx_ctx)
{
	if (gfx_ctx)
		*gfx_ctx = app->gfx_ctx;

	return app->api;
}

void *mty_window_get_native(MTY_App *app, MTY_Window window)
{
	return (void *) (uintptr_t) 0xCDD;
}


// Misc

void MTY_HotkeyToString(MTY_Mod mod, MTY_Key key, char *str, size_t len)
{
	memset(str, 0, len);

	MTY_Strcat(str, len, (mod & MTY_MOD_WIN) ? "Super+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_CTRL) ? "Ctrl+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_ALT) ? "Alt+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_SHIFT) ? "Shift+" : "");

	char key_str[32];
	if (web_get_key(key, key_str, 32))
		MTY_Strcat(str, len, key_str);
}

void MTY_SetAppID(const char *id)
{
}

void *MTY_GLGetProcAddress(const char *name)
{
	return NULL;
}
