// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define GFX_API_SUPPORTED(api) \
	((api) == MTY_GFX_GL)

#define GFX_DECLARE_TABLE()             \
	static const struct {               \
		GFX_DECLARE_API(_, GFX_FP)      \
	} GFX_API[] = {                     \
		GFX_DECLARE_ROW(GL, _gl_)       \
	};

#define GFX_UI_DECLARE_TABLE()             \
	static const struct {                  \
		GFX_UI_DECLARE_API(_, GFX_UI_FP)   \
	} GFX_UI_API[] = {                     \
		GFX_UI_DECLARE_ROW(GL, _gl_)       \
	};

#define GFX_CTX_DECLARE_TABLE()             \
	static const struct {                   \
		GFX_CTX_DECLARE_API(_, GFX_CTX_FP)  \
	} GFX_CTX_API[] = {                     \
		GFX_CTX_DECLARE_ROW(GL, _gl_)       \
	};
