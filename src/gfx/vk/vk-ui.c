// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ui.h"
GFX_UI_PROTOTYPES(_vk_)

struct vk_ui {
	char dummy;
};

struct gfx_ui *mty_vk_ui_create(MTY_Device *device)
{
	struct vk_ui *ctx = MTY_Alloc(1, sizeof(struct vk_ui));

	return (struct gfx_ui *) ctx;
}

bool mty_vk_ui_render(struct gfx_ui *gfx_ui, MTY_Device *device, MTY_Context *context,
	const MTY_DrawData *dd, MTY_Hash *cache, MTY_Surface *dest)
{
	return true;
}

void *mty_vk_ui_create_texture(MTY_Device *device, const void *rgba, uint32_t width, uint32_t height)
{
	return NULL;
}

void mty_vk_ui_destroy_texture(void **texture)
{
	if (!texture || !*texture)
		return;

	*texture = NULL;
}

void mty_vk_ui_destroy(struct gfx_ui **gfx_ui)
{
	if (!gfx_ui || !*gfx_ui)
		return;

	struct vk_ui *ctx = (struct vk_ui *) *gfx_ui;

	MTY_Free(ctx);
	*gfx_ui = NULL;
}
