// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DEFAULT_SOURCE // popen, pclose

#include "matoya.h"

#include <stdio.h>
#include <string.h>

#include "tlocal.h"

bool MTY_HasDialogs(void)
{
	return true;
}

void MTY_ShowMessageBox(const char *title, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char *msg = MTY_VsprintfD(fmt, args);

	va_end(args);

	// FIXME: Need to escape quotes
	const char *cmd = MTY_SprintfDL("zenity --info --title=\"%s\" --text=\"%s\"", title, msg);

	FILE *f = popen(cmd, "r");
	if (f)
		pclose(f);

	MTY_Free(msg);
}

const char *MTY_OpenFile(const char *title, MTY_App *app, MTY_Window window)
{
	char *name = NULL;

	FILE *f = popen("zenity --file-selection", "r");

	if (f) {
		name = mty_tlocal(MTY_PATH_MAX);

		if (!fgets(name, MTY_PATH_MAX, f)) {
			name = NULL;

		} else {
			// Zenity appends a newline, strip it
			name[strlen(name) - 1] = '\0';
		}

		pclose(f);
	}

	return name;
}
