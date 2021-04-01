// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

#include <sys/stat.h>

#define FSUTIL_DELIM '/'

static FILE *fsutil_open(const char *path, const char *mode)
{
	FILE *f = fopen(path, mode);
	if (!f) {
		MTY_Log("'fopen' failed to open '%s' with errno %d", MTY_GetFileName(path, true), errno);
		return NULL;
	}

	return f;
}

static size_t fsutil_size(const char *path)
{
	struct stat st;
	int32_t e = stat(path, &st);

	if (e != 0) {
		// Since these functions are lazily used to check the existence of files, don't log ENOENT
		if (errno != ENOENT)
			MTY_Log("'stat' failed to query '%s' with errno %d", MTY_GetFileName(path, true), errno);

		return 0;
	}

	return st.st_size;
}
