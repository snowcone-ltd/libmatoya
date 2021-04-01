// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>

#include "tlocal.h"

const char *MTY_GetPlatformString(uint32_t platform)
{
	uint8_t major = (platform & 0xFF00) >> 8;
	uint8_t minor = platform & 0xFF;

	char *ver = mty_tlocal(16);

	if (minor > 0) {
		snprintf(ver, 16, "%u.%u", major, minor);

	} else {
		snprintf(ver, 16, "%u", major);
	}

	return ver;
}

bool MTY_IsSupported(void)
{
	uint32_t platform = MTY_GetPlatform();

	MTY_OS os = platform & 0xFF000000;
	uint32_t major = (platform & 0xFF00) >> 8;
	uint32_t minor = platform & 0xFF;

	switch (os) {
		case MTY_OS_WINDOWS:
			return major > 6 || (major == 6 && minor >= 1);
		case MTY_OS_MACOS:
			return major > 10 || (major == 10 && minor > 11);
		case MTY_OS_ANDROID:
			return minor >= 26;
		case MTY_OS_LINUX:
		case MTY_OS_WEB:
			return true;
		case MTY_OS_IOS:
		case MTY_OS_TVOS:
		default:
			return false;
	}
}
