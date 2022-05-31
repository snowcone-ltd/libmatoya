// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static const MTLPixelFormat FMT_PLANE0[MTY_COLOR_FORMAT_MAX] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = MTLPixelFormatBGRA8Unorm,
	[MTY_COLOR_FORMAT_BGRA]       = MTLPixelFormatBGRA8Unorm,
	[MTY_COLOR_FORMAT_NV12]       = MTLPixelFormatR8Unorm,
	[MTY_COLOR_FORMAT_I420]       = MTLPixelFormatR8Unorm,
	[MTY_COLOR_FORMAT_I444]       = MTLPixelFormatR8Unorm,
	[MTY_COLOR_FORMAT_NV16]       = MTLPixelFormatR8Unorm,
	//[MTY_COLOR_FORMAT_BGR565]     = MTLPixelFormatB5G6R5Unorm,
	//[MTY_COLOR_FORMAT_BGRA5551]   = MTLPixelFormatBGR5A1Unorm,
	[MTY_COLOR_FORMAT_AYUV]       = MTLPixelFormatBGRA8Unorm,
	//[MTY_COLOR_FORMAT_Y410]       = MTLPixelFormatBGR10A2Unorm,
	[MTY_COLOR_FORMAT_Y416]       = MTLPixelFormatRGBA16Unorm,
	[MTY_COLOR_FORMAT_P010]       = MTLPixelFormatR16Unorm,
	[MTY_COLOR_FORMAT_P016]       = MTLPixelFormatR16Unorm,
	[MTY_COLOR_FORMAT_I444_10]    = MTLPixelFormatR16Unorm,
	[MTY_COLOR_FORMAT_I444_16]    = MTLPixelFormatR16Unorm,
};

static const MTLPixelFormat FMT_PLANE1[MTY_COLOR_FORMAT_MAX] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = MTLPixelFormatBGRA8Unorm,
	[MTY_COLOR_FORMAT_BGRA]       = MTLPixelFormatBGRA8Unorm,
	[MTY_COLOR_FORMAT_NV12]       = MTLPixelFormatRG8Unorm,
	[MTY_COLOR_FORMAT_I420]       = MTLPixelFormatR8Unorm,
	[MTY_COLOR_FORMAT_I444]       = MTLPixelFormatR8Unorm,
	[MTY_COLOR_FORMAT_NV16]       = MTLPixelFormatRG8Unorm,
	//[MTY_COLOR_FORMAT_BGR565]     = MTLPixelFormatB5G6R5Unorm,
	//[MTY_COLOR_FORMAT_BGRA5551]   = MTLPixelFormatBGR5A1Unorm,
	[MTY_COLOR_FORMAT_AYUV]       = MTLPixelFormatBGRA8Unorm,
	//[MTY_COLOR_FORMAT_Y410]       = MTLPixelFormatBGR10A2Unorm,
	[MTY_COLOR_FORMAT_Y416]       = MTLPixelFormatRGBA16Unorm,
	[MTY_COLOR_FORMAT_P010]       = MTLPixelFormatRG16Unorm,
	[MTY_COLOR_FORMAT_P016]       = MTLPixelFormatRG16Unorm,
	[MTY_COLOR_FORMAT_I444_10]    = MTLPixelFormatR16Unorm,
	[MTY_COLOR_FORMAT_I444_16]    = MTLPixelFormatR16Unorm,
};
