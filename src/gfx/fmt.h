// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define FMT_CONVERSION(fmt, full, multi) ( \
	(FMT_INFO[fmt].yuv ? 0x8 : 0) | \
	((multi)           ? 0x4 : 0) | \
	(FMT_INFO[fmt].ten ? 0x2 : 0) | \
	((full)            ? 0x1 : 0))

static const struct {
	uint8_t planes;
	uint8_t bpp;
	uint8_t yuv;
	uint8_t ten;

} FMT_INFO[MTY_COLOR_FORMAT_MAX] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = {0, 0, 0, 0},
	[MTY_COLOR_FORMAT_BGRA]       = {1, 4, 0, 0},
	[MTY_COLOR_FORMAT_BGR565]     = {1, 2, 0, 0},
	[MTY_COLOR_FORMAT_BGRA5551]   = {1, 2, 0, 0},
	[MTY_COLOR_FORMAT_AYUV]       = {1, 4, 1, 0},
	[MTY_COLOR_FORMAT_Y410]       = {1, 4, 1, 1},
	[MTY_COLOR_FORMAT_Y416]       = {1, 8, 1, 1},
	[MTY_COLOR_FORMAT_2PLANES]    = {2, 1, 1, 0},
	[MTY_COLOR_FORMAT_3PLANES]    = {3, 1, 1, 0},
	[MTY_COLOR_FORMAT_2PLANES_16] = {2, 2, 1, 1},
	[MTY_COLOR_FORMAT_3PLANES_16] = {3, 2, 1, 1},
};

static bool fmt_reload_textures(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const uint8_t *image, const MTY_RenderDesc *desc, bool (*refresh_resource)(struct gfx *gfx,
	MTY_Device *device, MTY_Context *context, MTY_ColorFormat fmt, uint8_t plane, const uint8_t *image,
	uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp))
{
	uint8_t bpp = FMT_INFO[desc->format].bpp;
	uint8_t planes = FMT_INFO[desc->format].planes;

	uint32_t _hdiv = desc->chroma == MTY_CHROMA_420 ? 2 : 1;
	uint32_t _wdiv = desc->chroma != MTY_CHROMA_444 ? 2 : 1;

	for (uint8_t x = 0; x < planes; x++) {
		// First plane (usually Y) is always full width, full height
		uint32_t hdiv = x > 0 ? _hdiv : 1;
		uint32_t wdiv = x > 0 ? _wdiv : 1;

		// The second plane of two plane formats is always packed
		uint8_t pack = x == 1 && planes == 2 ? 2 : 1;

		if (!refresh_resource(gfx, device, context, desc->format, x, image, desc->imageWidth / wdiv,
			desc->cropWidth / wdiv, desc->cropHeight / hdiv, pack * bpp))
			return false;

		image += (desc->imageWidth / wdiv) * (desc->imageHeight / hdiv) * bpp;
	}

	return true;
}
