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
static TLOCAL float TIME_FREQUENCY;

MTY_Time MTY_GetTime(void)
{
	return mty_timestamp();
}

float MTY_TimeDiff(MTY_Time begin, MTY_Time end)
{
	if (!TIME_FREQ_INIT) {
		TIME_FREQUENCY = mty_frequency();
		TIME_FREQ_INIT = true;
	}

	return (float) (end - begin) * TIME_FREQUENCY;
}

void MTY_Sleep(uint32_t timeout)
{
	mty_sleep(timeout);
}

void MTY_SetTimerResolution(uint32_t res)
{
}

void MTY_RevertTimerResolution(uint32_t res)
{
}
