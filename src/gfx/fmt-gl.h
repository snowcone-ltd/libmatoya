// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static const GLenum FMT_PLANES[MTY_COLOR_FORMAT_MAX][3][3] = {
	[MTY_COLOR_FORMAT_UNKNOWN]    = {{0,           0,       0},                              {0},                                  {0}},
	[MTY_COLOR_FORMAT_BGRA]       = {{GL_RGBA,     GL_BGRA, GL_UNSIGNED_BYTE},               {0},                                  {0}},
	[MTY_COLOR_FORMAT_BGR565]     = {{GL_RGB,      GL_RGB,  GL_UNSIGNED_SHORT_5_6_5},        {0},                                  {0}},
	[MTY_COLOR_FORMAT_BGRA5551]   = {{GL_RGBA,     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},  {0},                                  {0}},
	[MTY_COLOR_FORMAT_AYUV]       = {{GL_RGBA,     GL_BGRA, GL_UNSIGNED_BYTE},               {0},                                  {0}},
	[MTY_COLOR_FORMAT_Y410]       = {{GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV}, {0},                                  {0}},
	[MTY_COLOR_FORMAT_Y416]       = {{GL_RGBA16,   GL_BGRA, GL_UNSIGNED_SHORT},              {0},                                  {0}},
	[MTY_COLOR_FORMAT_2PLANES]    = {{GL_R8,       GL_RED,  GL_UNSIGNED_BYTE},               {GL_RG8,  GL_RG,  GL_UNSIGNED_BYTE},  {0}},
	[MTY_COLOR_FORMAT_3PLANES]    = {{GL_R8,       GL_RED,  GL_UNSIGNED_BYTE},               {GL_R8,   GL_RED, GL_UNSIGNED_BYTE},  {GL_R8, GL_RED, GL_UNSIGNED_BYTE}},
	[MTY_COLOR_FORMAT_2PLANES_16] = {{GL_R16,      GL_RED,  GL_UNSIGNED_SHORT},              {GL_RG16, GL_RG,  GL_UNSIGNED_SHORT}, {0}},
	[MTY_COLOR_FORMAT_3PLANES_16] = {{GL_R16,      GL_RED,  GL_UNSIGNED_SHORT},              {GL_R16,  GL_RED, GL_UNSIGNED_SHORT}, {GL_R16, GL_RED, GL_UNSIGNED_SHORT}},
};
