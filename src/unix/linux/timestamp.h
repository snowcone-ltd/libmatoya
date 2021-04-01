// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <time.h>
#include <errno.h>

static int64_t mty_timestamp(void)
{
	struct timespec ts = {0};
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		MTY_Log("'clock_gettime' failed with errno %d", errno);

	// XXX time_t can be 32 bits and multiplying it by 1000000
	// can overflow it. Make sure to cast it as 64-bits for multipication
	uint64_t r = (uint64_t) ts.tv_sec * 1000 * 1000;
	r += ts.tv_nsec / 1000;

	return r;
}

static float mty_frequency(void)
{
	return 0.001f;
}
