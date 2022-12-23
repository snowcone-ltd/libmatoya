// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DARWIN_C_SOURCE // CLOCK_MONOTONIC_RAW

#include "matoya.h"

#include <time.h>
#include <errno.h>

MTY_Time MTY_GetTime(void)
{
	struct timespec ts = {0};
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0)
		MTY_Log("'clock_gettime' failed with errno %d", errno);

	// XXX time_t can be 32 bits and multiplying it by 1000000
	// can overflow it. Make sure to cast it as 64-bits for multipication
	uint64_t r = (uint64_t) ts.tv_sec * 1000 * 1000;
	r += ts.tv_nsec / 1000;

	return r;
}

double MTY_TimeDiff(MTY_Time begin, MTY_Time end)
{
	return (end - begin) / 1000.0;
}

void MTY_Sleep(uint32_t timeout)
{
	struct timespec ts = {0};
	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000 * 1000;

	if (nanosleep(&ts, NULL) != 0)
		MTY_Log("'nanosleep' failed with errno %d", errno);
}

void MTY_PreciseSleep(double timeout, double spin)
{
	// Inspired by https://blog.bearcats.nl/accurate-sleep-function/

	MTY_Time start = MTY_GetTime();

	for (double ep = 0.0; ep < timeout - spin; ep = MTY_TimeDiff(start, MTY_GetTime()))
		MTY_Sleep(1);

	while (MTY_TimeDiff(start, MTY_GetTime()) < timeout);
}

void MTY_SetTimerResolution(uint32_t res)
{
}

void MTY_RevertTimerResolution(uint32_t res)
{
}
