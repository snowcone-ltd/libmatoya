// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <pwd.h>

static bool home_get_dir(char *dir, size_t size)
{
	const struct passwd *pw = getpwuid(getuid());

	if (pw) {
		snprintf(dir, size, "%s", pw->pw_dir);
		return true;
	}

	MTY_Log("'getpwuid' failed with errno %d", errno);

	return false;
}
