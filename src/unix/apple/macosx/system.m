// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <AppKit/AppKit.h>
#include <mach-o/dyld.h>

#include "tlocal.h"

uint32_t MTY_GetPlatform(void)
{
	uint32_t v = MTY_OS_MACOS;

	NSProcessInfo *pInfo = [NSProcessInfo processInfo];
	NSOperatingSystemVersion version = [pInfo operatingSystemVersion];

	v |= (uint32_t) version.majorVersion << 8;
	v |= (uint32_t) version.minorVersion;

	return v;
}

uint32_t MTY_GetPlatformNoWeb(void)
{
	return MTY_GetPlatform();
}

void MTY_HandleProtocol(const char *uri, void *token)
{
	NSString *nsuri = [NSString stringWithUTF8String:uri];

	if (strstr(uri, "http") == uri) {
		[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:nsuri]];

	} else {
		[[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:nsuri]];
	}
}

const char *MTY_GetProcessPath(void)
{
	char tmp[MTY_PATH_MAX] = {0};
	uint32_t bsize = MTY_PATH_MAX - 1;

	int32_t e = _NSGetExecutablePath(tmp, &bsize);
	if (e != 0) {
		MTY_Log("'_NSGetExecutablePath' failed with error %d", e);
		return "/app";
	}

	return mty_tlocal_strcpy(tmp);
}

bool MTY_RestartProcess(char * const *argv)
{
	execv(MTY_GetProcessPath(), argv);
	MTY_Log("'execv' failed with errno %d", errno);

	return false;
}

void *MTY_GetJNIEnv(void)
{
	return NULL;
}
