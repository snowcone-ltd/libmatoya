// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DEFAULT_SOURCE // readlink

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>

#include "tlocal.h"

const char *MTY_GetSOExtension(void)
{
	return "so";
}

uint32_t MTY_GetPlatform(void)
{
	return MTY_OS_LINUX;
}

uint32_t MTY_GetPlatformNoWeb(void)
{
	return MTY_GetPlatform();
}

void MTY_HandleProtocol(const char *uri, void *token)
{
	char *cmd = MTY_SprintfD("xdg-open \"%s\" 2> /dev/null &", uri);

	if (system(cmd) == -1)
		MTY_Log("'system' failed with errno %d", errno);

	MTY_Free(cmd);
}

const char *MTY_GetProcessPath(void)
{
	char tmp[MTY_PATH_MAX] = {0};

	ssize_t n = readlink("/proc/self/exe", tmp, MTY_PATH_MAX - 1);
	if (n < 0) {
		MTY_Log("'readlink' failed with errno %d", errno);
		return "/app";
	}

	return mty_tlocal_strcpy(tmp);
}

bool MTY_StartInProcess(const char *path, char * const *argv, const char *dir)
{
	if (dir) {
		if (chdir(dir) == -1) {
			MTY_Log("'chdir' failed with errno %d", errno);
			return false;
		}
	}

	if (!path)
		path = MTY_GetProcessPath();

	execv(path, argv);
	MTY_Log("'execv' failed with errno %d", errno);

	return false;
}

bool MTY_RestartProcess(char * const *argv)
{
	return MTY_StartInProcess(NULL, argv, NULL);
}

void *MTY_GetJNIEnv(void)
{
	return NULL;
}
