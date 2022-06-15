// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static const MTLPixelFormat FMT_PLANES[MTY_COLOR_FORMAT_MAX][3] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = {MTLPixelFormatInvalid,      MTLPixelFormatInvalid,   MTLPixelFormatInvalid},
	[MTY_COLOR_FORMAT_BGRA]       = {MTLPixelFormatBGRA8Unorm,   MTLPixelFormatInvalid,   MTLPixelFormatInvalid},
	[MTY_COLOR_FORMAT_RGBA]       = {MTLPixelFormatRGBA8Unorm,   MTLPixelFormatInvalid,   MTLPixelFormatInvalid},
	// [MTY_COLOR_FORMAT_BGR565]     = {MTLPixelFormatB5G6R5Unorm,  MTLPixelFormatInvalid,   MTLPixelFormatInvalid},
	// [MTY_COLOR_FORMAT_BGRA5551]   = {MTLPixelFormatBGR5A1Unorm,  MTLPixelFormatInvalid,   MTLPixelFormatInvalid},
	[MTY_COLOR_FORMAT_AYUV]       = {MTLPixelFormatBGRA8Unorm,   MTLPixelFormatInvalid,   MTLPixelFormatInvalid},
	// [MTY_COLOR_FORMAT_Y410]       = {MTLPixelFormatBGR10A2Unorm, MTLPixelFormatInvalid,   MTLPixelFormatInvalid},
	[MTY_COLOR_FORMAT_Y416]       = {MTLPixelFormatRGBA16Unorm,  MTLPixelFormatInvalid,   MTLPixelFormatInvalid},
	[MTY_COLOR_FORMAT_2PLANES]    = {MTLPixelFormatR8Unorm,      MTLPixelFormatRG8Unorm,  MTLPixelFormatInvalid},
	[MTY_COLOR_FORMAT_3PLANES]    = {MTLPixelFormatR8Unorm,      MTLPixelFormatR8Unorm,   MTLPixelFormatR8Unorm},
	[MTY_COLOR_FORMAT_2PLANES_16] = {MTLPixelFormatR16Unorm,     MTLPixelFormatRG16Unorm, MTLPixelFormatInvalid},
	[MTY_COLOR_FORMAT_3PLANES_16] = {MTLPixelFormatR16Unorm,     MTLPixelFormatR16Unorm,  MTLPixelFormatR16Unorm},
};
