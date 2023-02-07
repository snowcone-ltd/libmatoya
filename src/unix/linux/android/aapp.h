// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdint.h>
#include <stdbool.h>

void *mty_app_get_obj(void);
uint32_t mty_app_get_kb_height(void);
void mty_gfx_size(uint32_t *width, uint32_t *height);
bool mty_gfx_is_ready(void);
void mty_gfx_lock(void);
void mty_gfx_unlock(void);

void *mty_gfx_wait_for_window(int32_t timeout);
void mty_gfx_done_with_window(void);
