// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>

#include "tlocal.h"

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

static const char *system_get_os_string(uint32_t platform)
{
	switch (platform & 0xFF000000) {
		case MTY_OS_WINDOWS: return "Windows";
		case MTY_OS_MACOS:   return "macOS";
		case MTY_OS_ANDROID: return "Android";
		case MTY_OS_LINUX:   return "Linux";
		case MTY_OS_WEB:     return "Web";
		case MTY_OS_IOS:     return "iOS";
		case MTY_OS_TVOS:    return "tvOS";
	}

	return "Unknown";
}

const char *MTY_GetPlatformString(uint32_t platform)
{
	MTY_OS os = platform & 0xFF000000;

	uint8_t major = (platform & 0xFF00) >> 8;
	uint8_t minor = platform & 0xFF;

	char *final = mty_tlocal(64);

	if (os != MTY_OS_UNKNOWN)
		MTY_Strcat(final, 64, system_get_os_string(platform));

	if (major > 0 || minor > 0) {
		if (os != MTY_OS_UNKNOWN)
			MTY_Strcat(final, 64, " ");

		if (minor > 0) {
			MTY_Strcat(final, 64, MTY_SprintfDL("%u.%u", major, minor));

		} else {
			MTY_Strcat(final, 64, MTY_SprintfDL("%u", major));
		}
	}

	return final;
}

const char *MTY_GetProcessDir(void)
{
	return MTY_GetPathPrefix(MTY_GetProcessPath());
}
