// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod.h"
GFX_PROTOTYPES(_vk_)

struct vk {
	char dummy;
};

struct gfx *mty_vk_create(MTY_Device *device)
{
	struct vk *ctx = MTY_Alloc(1, sizeof(struct vk));

	return (struct gfx *) ctx;
}

bool mty_vk_render(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest)
{
	return true;
}

void mty_vk_destroy(struct gfx **gfx)
{
	if (!gfx || !*gfx)
		return;

	struct vk *ctx = (struct vk *) *gfx;

	MTY_Free(ctx);
	*gfx = NULL;
}

void *mty_vk_get_state(MTY_Device *device, MTY_Context *_context)
{
	return NULL;
}

void mty_vk_set_state(MTY_Device *device, MTY_Context *_context, void *state)
{
}

void mty_vk_free_state(void **state)
{
}
