// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

void mty_gfx_global_init(void);
void mty_gfx_global_destroy(void);
void mty_gfx_set_dims(uint32_t width, uint32_t height);
bool mty_gfx_is_ready(void);
void mty_gfx_size(uint32_t *width, uint32_t *height);
void mty_gfx_set_kb_height(int32_t height);
MTY_ContextState mty_gfx_state(void);
