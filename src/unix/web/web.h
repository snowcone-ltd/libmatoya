// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

// This interface is implemented in matoya.js (JavaScript)

typedef void *(*WEB_ALLOC)(size_t n, size_t size);
typedef void (*WEB_FREE)(void *ptr);
typedef void (*WEB_CONTROLLER)(MTY_App *ctx, uint32_t id, uint32_t state, uint32_t buttons,
	float lx, float ly, float rx, float ry, float lt, float rt);
typedef void (*WEB_MOVE)(MTY_App *ctx);
typedef void (*WEB_MOTION)(MTY_App *ctx, bool relative, int32_t x, int32_t y);
typedef void (*WEB_BUTTON)(MTY_App *ctx, bool pressed, int32_t button, int32_t x, int32_t y);
typedef void (*WEB_SCROLL)(MTY_App *ctx, int32_t x, int32_t y);
typedef bool (*WEB_KEY)(MTY_App *ctx, bool pressed, MTY_Key key, const char *text, uint32_t mods);
typedef void (*WEB_FOCUS)(MTY_App *ctx, bool focus);
typedef void (*WEB_DROP)(MTY_App *ctx, const char *name, const void *data, size_t size);
typedef void (*WEB_RESIZE)(MTY_App *ctx);

bool web_has_focus(void);
void web_alert(const char *title, const char *msg);
void web_platform(char *platform, size_t size);
void web_set_fullscreen(bool fullscreen);
bool web_get_fullscreen(void);
bool web_get_key(MTY_Key key, char *str, size_t len);
void web_set_key(bool reverse, const char *code, MTY_Key key);
bool web_is_visible(void);
void web_wake_lock(bool enable);
void web_rumble_gamepad(uint32_t id, float low, float high);
void web_show_cursor(bool show);
void web_use_default_cursor(bool use_default);
void web_set_png_cursor(const void *image, size_t size, uint32_t hotX, uint32_t hotY);
void web_set_pointer_lock(bool enable);
bool web_get_relative(void);
char *web_get_clipboard(void);
void web_set_clipboard(const char *text);
void web_set_mem_funcs(WEB_ALLOC alloc, WEB_FREE free);
void web_get_size(uint32_t *width, uint32_t *height);
void web_get_position(int32_t *x, int32_t *y);
void web_get_screen_size(uint32_t *width, uint32_t *height);
void web_set_title(const char *title);
void web_raf(MTY_App *app, MTY_AppFunc func, WEB_CONTROLLER controller, WEB_MOVE move, void *opaque);
void web_register_drag(void);
void web_gl_flush(void);
void web_set_swap_interval(uint32_t interval);
float web_get_pixel_ratio(void);
void web_attach_events(MTY_App *app, WEB_MOTION motion, WEB_BUTTON button,
	WEB_SCROLL scroll, WEB_KEY key, WEB_FOCUS focus, WEB_DROP drop, WEB_RESIZE resize);
