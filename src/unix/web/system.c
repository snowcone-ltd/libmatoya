// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>

#include "web.h"

const char *MTY_GetSOExtension(void)
{
	return "so";
}

uint32_t MTY_GetPlatform(void)
{
	return MTY_OS_WEB;
}

uint32_t MTY_GetPlatformNoWeb(void)
{
	char platform[64] = {0};
	web_platform(platform, 64);

	if (strstr(platform, "Win32"))
		return MTY_OS_WINDOWS;

	if (strstr(platform, "Mac"))
		return MTY_OS_MACOS;

	if (strstr(platform, "Android"))
		return MTY_OS_ANDROID;

	if (strstr(platform, "Linux"))
		return MTY_OS_LINUX;

	if (strstr(platform, "iPhone"))
		return MTY_OS_IOS;

	return MTY_OS_UNKNOWN;
}

const char *MTY_GetProcessPath(void)
{
	return "/app";
}

bool MTY_RestartProcess(char * const *argv)
{
	return false;
}

void *MTY_GetJNIEnv(void)
{
	return NULL;
}
