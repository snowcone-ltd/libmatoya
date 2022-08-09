// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#if defined(MTY_GL_ES)
	#define GL_SHADER_VERSION "#version 100\n"
#else
	#define GL_SHADER_VERSION "#version 110\n"
#endif

#if defined MTY_GL_EXTERNAL

#if defined(MTY_GL_INCLUDE)
	#include MTY_GL_INCLUDE
#else
	#define GL_GLEXT_PROTOTYPES
	#include "glcorearb.h"
#endif

#else

#include "glcorearb.h"

#endif
