// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define FMT_CONVERSION(fmt, full) \
	(FMT_YUV[fmt] * ((full) ? 4 : 1))

enum fmt_planes {
	FMT_UNKNOWN = 0,
	FMT_1_PLANE = 1,
	FMT_2_PLANE = 2,
	FMT_3_PLANE = 3,
};

static const enum fmt_planes FMT_PLANES[MTY_COLOR_FORMAT_MAX] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = FMT_UNKNOWN,
	[MTY_COLOR_FORMAT_BGRA]       = FMT_1_PLANE,
	[MTY_COLOR_FORMAT_NV12]       = FMT_2_PLANE,
	[MTY_COLOR_FORMAT_I420]       = FMT_3_PLANE,
	[MTY_COLOR_FORMAT_I444]       = FMT_3_PLANE,
	[MTY_COLOR_FORMAT_NV16]       = FMT_2_PLANE,
	[MTY_COLOR_FORMAT_BGR565]     = FMT_1_PLANE,
	[MTY_COLOR_FORMAT_BGRA5551]   = FMT_1_PLANE,
	[MTY_COLOR_FORMAT_AYUV]       = FMT_1_PLANE,
	[MTY_COLOR_FORMAT_Y410]       = FMT_1_PLANE,
	[MTY_COLOR_FORMAT_P010]       = FMT_2_PLANE,
	[MTY_COLOR_FORMAT_P016]       = FMT_2_PLANE,
	[MTY_COLOR_FORMAT_I444_10]    = FMT_3_PLANE,
	[MTY_COLOR_FORMAT_I444_16]    = FMT_3_PLANE,
};

static const uint32_t FMT_DIV[MTY_COLOR_FORMAT_MAX] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = 1,
	[MTY_COLOR_FORMAT_BGRA]       = 1,
	[MTY_COLOR_FORMAT_NV12]       = 2,
	[MTY_COLOR_FORMAT_I420]       = 2,
	[MTY_COLOR_FORMAT_I444]       = 1,
	[MTY_COLOR_FORMAT_NV16]       = 1,
	[MTY_COLOR_FORMAT_BGR565]     = 1,
	[MTY_COLOR_FORMAT_BGRA5551]   = 1,
	[MTY_COLOR_FORMAT_AYUV]       = 1,
	[MTY_COLOR_FORMAT_Y410]       = 1,
	[MTY_COLOR_FORMAT_Y416]       = 1,
	[MTY_COLOR_FORMAT_P010]       = 2,
	[MTY_COLOR_FORMAT_P016]       = 2,
	[MTY_COLOR_FORMAT_I444_10]    = 1,
	[MTY_COLOR_FORMAT_I444_16]    = 1,
};

static const int8_t FMT_BPP[MTY_COLOR_FORMAT_MAX] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = 4,
	[MTY_COLOR_FORMAT_BGRA]       = 4,
	[MTY_COLOR_FORMAT_NV12]       = 1,
	[MTY_COLOR_FORMAT_I420]       = 1,
	[MTY_COLOR_FORMAT_I444]       = 1,
	[MTY_COLOR_FORMAT_NV16]       = 1,
	[MTY_COLOR_FORMAT_BGR565]     = 2,
	[MTY_COLOR_FORMAT_BGRA5551]   = 2,
	[MTY_COLOR_FORMAT_AYUV]       = 4,
	[MTY_COLOR_FORMAT_Y410]       = 4,
	[MTY_COLOR_FORMAT_Y416]       = 8,
	[MTY_COLOR_FORMAT_P010]       = 2,
	[MTY_COLOR_FORMAT_P016]       = 2,
	[MTY_COLOR_FORMAT_I444_10]    = 2,
	[MTY_COLOR_FORMAT_I444_16]    = 2,
};

static const uint32_t FMT_YUV[MTY_COLOR_FORMAT_MAX] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = 0,
	[MTY_COLOR_FORMAT_BGRA]       = 0,
	[MTY_COLOR_FORMAT_NV12]       = 1,
	[MTY_COLOR_FORMAT_I420]       = 1,
	[MTY_COLOR_FORMAT_I444]       = 1,
	[MTY_COLOR_FORMAT_NV16]       = 1,
	[MTY_COLOR_FORMAT_BGR565]     = 0,
	[MTY_COLOR_FORMAT_BGRA5551]   = 0,
	[MTY_COLOR_FORMAT_AYUV]       = 1,
	[MTY_COLOR_FORMAT_Y410]       = 1,
	[MTY_COLOR_FORMAT_Y416]       = 1,
	[MTY_COLOR_FORMAT_P010]       = 2,
	[MTY_COLOR_FORMAT_P016]       = 1,
	[MTY_COLOR_FORMAT_I444_10]    = 2,
	[MTY_COLOR_FORMAT_I444_16]    = 1,
};
