// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>
#include <stdio.h>

#include "tlocal.h"
#include "web.h"

MTY_SO *MTY_SOLoad(const char *path)
{
	return NULL;
}

void *MTY_SOGetSymbol(MTY_SO *so, const char *name)
{
	return NULL;
}

void MTY_SOUnload(MTY_SO **so)
{
	if (!so || !*so)
		return;

	*so = NULL;
}

const char *MTY_GetSOExtension(void)
{
	return "wasm";
}

const char *MTY_GetHostname(void)
{
	char *hostname = web_get_hostname();
	const char *local = mty_tlocal_strcpy(hostname);

	MTY_Free(hostname);

	return local;
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

bool MTY_StartInProcess(const char *path, const char * const *argv, const char *dir)
{
	return false;
}

bool MTY_RestartProcess(char * const *argv)
{
	return MTY_StartInProcess(NULL, argv, NULL);
}

void MTY_SetCrashFunc(MTY_CrashFunc func, void *opaque)
{
}

void MTY_OpenConsole(const char *title)
{
}

void MTY_CloseConsole(void)
{
}

bool MTY_GetRunOnStartup(const char *name)
{
	return false;
}

void MTY_SetRunOnStartup(const char *name, const char *path, const char *args)
{
}

void *MTY_GetJNIEnv(void)
{
	return NULL;
}


// Exports

__attribute__((export_name("mty_system_alloc")))
void *mty_system_alloc(size_t len, size_t size)
{
	return MTY_Alloc(len, size);
}

__attribute__((export_name("mty_system_free")))
void mty_system_free(void *mem)
{
	MTY_Free(mem);
}

__attribute__((export_name("mty_system_disable_buffering")))
void mty_system_disable_buffering(void)
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
}
