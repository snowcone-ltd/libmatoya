// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static bool home_get_dir(char *dir, size_t size)
{
	snprintf(dir, size, "/");

	return true;
}
