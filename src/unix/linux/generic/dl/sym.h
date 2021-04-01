// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

#define LOAD_SYM(so, name) \
	name = MTY_SOGetSymbol(so, #name); \
	if (!name) {r = false; goto except;}

#define LOAD_SYM_OPT(so, name) \
	name = MTY_SOGetSymbol(so, #name);
