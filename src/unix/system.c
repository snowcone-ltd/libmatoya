// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include "tlocal.h"
#include "dlopen.h"

static MTY_CrashFunc SYSTEM_CRASH_FUNC;
static void *SYSTEM_OPAQUE;

MTY_SO *MTY_SOLoad(const char *path)
{
	MTY_SO *so = dlopen(path, DLOPEN_FLAGS);
	if (!so) {
		const char *estr = dlerror();
		if (estr)
			MTY_Log("%s", estr);

		MTY_Log("'dlopen' failed to find '%s'", path);
	}

	return so;
}

void *MTY_SOGetSymbol(MTY_SO *so, const char *name)
{
	void *sym = dlsym(so, name);
	if (!sym) {
		const char *estr = dlerror();
		if (estr)
			MTY_Log("%s", estr);

		MTY_Log("'dlsym' failed to find '%s'", name);
	}

	return sym;
}

void MTY_SOUnload(MTY_SO **so)
{
	if (!so || !*so)
		return;

	int32_t e = dlclose(*so);
	if (e != 0) {
		const char *estr = dlerror();
		if (estr) {
			MTY_Log("'dlclose' failed with error %d: '%s'", e, estr);

		} else {
			MTY_Log("'dlclose' failed with error %d", e);
		}
	}

	*so = NULL;
}

const char *MTY_GetHostname(void)
{
	char tmp[MTY_PATH_MAX] = {0};

	int32_t e = gethostname(tmp, MTY_PATH_MAX - 1);
	if (e != 0) {
		MTY_Log("'gethostname' failed with errno %d", errno);
		return "noname";
	}

	return mty_tlocal_strcpy(tmp);
}

static void system_signal_handler(int32_t sig)
{
	if (SYSTEM_CRASH_FUNC)
		SYSTEM_CRASH_FUNC(sig == SIGTERM || sig == SIGINT, SYSTEM_OPAQUE);

	signal(sig, SIG_DFL);
	raise(sig);
}

void MTY_SetCrashFunc(MTY_CrashFunc func, void *opaque)
{
	signal(SIGINT, system_signal_handler);
	signal(SIGSEGV, system_signal_handler);
	signal(SIGABRT, system_signal_handler);
	signal(SIGTERM, system_signal_handler);

	SYSTEM_CRASH_FUNC = func;
	SYSTEM_OPAQUE = opaque;
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
