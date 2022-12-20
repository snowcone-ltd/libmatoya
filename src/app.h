// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"
#include "gfx/mod-ctx.h"

#define APP_DEFAULT_FRAME() \
	MTY_MakeDefaultFrame(0, 0, 800, 600, 1.0f);

void mty_window_set_gfx(MTY_App *app, MTY_Window window, MTY_GFX api, struct gfx_ctx *gfx_ctx);
MTY_GFX mty_window_get_gfx(MTY_App *app, MTY_Window window, struct gfx_ctx **gfx_ctx);
MTY_Frame mty_window_adjust(uint32_t screen_w, uint32_t screen_h, float scale, float max_h,
	int32_t x, int32_t y, uint32_t w, uint32_t h);

// Android specific
void *mty_app_get_obj(void);
uint32_t mty_app_get_kb_height(void);
void mty_gfx_size(uint32_t *width, uint32_t *height);
bool mty_gfx_is_ready(void);
void mty_gfx_lock(void);
void mty_gfx_unlock(void);

void *mty_gfx_wait_for_window(int32_t timeout);
void mty_gfx_done_with_window(void);
