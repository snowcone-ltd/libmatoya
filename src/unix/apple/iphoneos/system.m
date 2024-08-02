// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

const char *MTY_GetSOExtension(void)
{
	return "dylib";
}

uint32_t MTY_GetPlatform(void)
{
	return MTY_OS_UNKNOWN;
}

uint32_t MTY_GetPlatformNoWeb(void)
{
	return MTY_GetPlatform();
}

void MTY_HandleProtocol(const char *uri, void *token)
{
}

const char *MTY_GetProcessPath(void)
{
	return "/app";
}

bool MTY_StartInProcess(const char *path, char * const *argv, const char *dir)
{
	return false;
}

bool MTY_RestartProcess(char * const *argv)
{
	return MTY_StartInProcess(NULL, argv, NULL);
}
