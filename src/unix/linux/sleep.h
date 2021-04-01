// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <time.h>
#include <errno.h>

static void mty_sleep(uint32_t timeout)
{
	struct timespec ts = {0};
	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000 * 1000;

	if (nanosleep(&ts, NULL) != 0)
		MTY_Log("'nanosleep' failed with errno %d", errno);
}
