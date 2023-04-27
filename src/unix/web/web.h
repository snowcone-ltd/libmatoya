// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

// This interface is implemented in matoya.js and matoya-worker.js (JavaScript)

void web_alert(const char *title, const char *msg);
void web_platform(char *platform, size_t size);
void web_set_fullscreen(bool fullscreen);
bool web_get_key(MTY_Key key, char *str, size_t len);
void web_set_key(bool reverse, const char *code, MTY_Key key);
void web_wake_lock(bool enable);
void web_rumble_gamepad(uint32_t id, float low, float high);
void web_show_cursor(bool show);
void web_use_default_cursor(bool use_default);
void web_set_png_cursor(const void *image, size_t size, uint32_t hotX, uint32_t hotY);
void web_set_pointer_lock(bool enable);
char *web_get_hostname(void);
char *web_get_clipboard(void);
void web_set_clipboard(const char *text);
void web_set_title(const char *title);
void web_run_and_yield(MTY_IterFunc iter, void *opaque);
void web_register_drag(void);
void web_gl_flush(void);
void web_set_gfx(void);
void web_set_canvas_size(uint32_t width, uint32_t height);
void web_present(bool wait);
void web_set_app(MTY_App *ctx);
