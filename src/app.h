// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"
#include "webview.h"
#include "gfx/mod-ctx.h"

#define APP_DEFAULT_FRAME() \
	MTY_MakeDefaultFrame(0, 0, 800, 600, 1.0f);

struct window_common {
	MTY_Renderer *renderer;
	struct webview *webview;
	struct gfx_ctx *gfx_ctx;
	MTY_GFX api;
};

// App
MTY_EventFunc mty_app_get_event_func(MTY_App *app, void **opaque);
MTY_Hash *mty_app_get_hotkey_hash(MTY_App *app);

// Window
struct window_common *mty_window_get_common(MTY_App *app, MTY_Window window);

// Window sizing
MTY_Frame mty_window_adjust(uint32_t screen_w, uint32_t screen_h, float scale, float max_h,
	int32_t x, int32_t y, uint32_t w, uint32_t h);

// Hotkeys
void mty_app_kb_to_hotkey(MTY_App *app, MTY_Event *evt, MTY_EventType type);
