// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <time.h>
#include <errno.h>

static void mty_get_time(struct timespec *ts)
{
	if (clock_gettime(CLOCK_REALTIME, ts) != 0)
		MTY_LogFatal("'clock_gettime' failed with errno %d", errno);
}
