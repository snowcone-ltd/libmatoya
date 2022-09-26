// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>

#include "tlocal.h"

static void log_none(const char *msg, void *opaque);

static MTY_Atomic32 LOG_DISABLED;
static MTY_LogFunc LOG_FUNC = log_none;
static void *LOG_OPAQUE;

static TLOCAL char *LOG_MSG;
static TLOCAL bool LOG_PREVENT_RECURSIVE;

static void log_none(const char *msg, void *opaque)
{
}

static void log_internal(const char *func, const char *fmt, va_list args)
{
	if (LOG_PREVENT_RECURSIVE)
		return;

	char *fmt_name = MTY_SprintfD("%s: %s", func, fmt);
	char *msg = MTY_VsprintfD(fmt_name, args);

	LOG_MSG = mty_tlocal_strcpy(msg);

	MTY_Free(msg);
	MTY_Free(fmt_name);

	if (!MTY_Atomic32Get(&LOG_DISABLED)) {
		LOG_PREVENT_RECURSIVE = true;
		LOG_FUNC(LOG_MSG, LOG_OPAQUE);
		LOG_PREVENT_RECURSIVE = false;
	}
}

const char *MTY_GetLog(void)
{
	return LOG_MSG ? LOG_MSG : "";
}

void MTY_SetLogFunc(MTY_LogFunc func, void *opaque)
{
	LOG_FUNC = func ? func : log_none;
	LOG_OPAQUE = opaque;
}

void MTY_DisableLog(bool disabled)
{
	MTY_Atomic32Set(&LOG_DISABLED, disabled ? 1 : 0);
}

void MTY_LogParams(const char *func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal(func, fmt, args);
	va_end(args);
}

void MTY_LogFatalParams(const char *func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_internal(func, fmt, args);
	va_end(args);

	_Exit(EXIT_FAILURE);
}
