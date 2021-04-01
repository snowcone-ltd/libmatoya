// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

uint32_t MTY_GetVersion(void)
{
	return MTY_VERSION_MAJOR << 8 | MTY_VERSION_MINOR;
}
