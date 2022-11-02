// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "tlocal.h"
#include "sleep.h"
#include "timestamp.h"

static TLOCAL bool TIME_FREQ_INIT;
static TLOCAL double TIME_FREQUENCY;

MTY_Time MTY_GetTime(void)
{
	return mty_timestamp();
}

double MTY_TimeDiff(MTY_Time begin, MTY_Time end)
{
	if (!TIME_FREQ_INIT) {
		TIME_FREQUENCY = mty_frequency();
		TIME_FREQ_INIT = true;
	}

	return (end - begin) * TIME_FREQUENCY;
}

void MTY_Sleep(uint32_t timeout)
{
	mty_sleep(timeout);
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
