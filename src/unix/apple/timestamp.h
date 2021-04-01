// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <mach/mach_time.h>

static int64_t mty_timestamp(void)
{
	return mach_absolute_time();
}

static float mty_frequency(void)
{
	mach_timebase_info_data_t timebase;
	kern_return_t e = mach_timebase_info(&timebase);
	if (e != KERN_SUCCESS)
		MTY_LogFatal("'mach_timebase_info' failed with error %d", e);

	return timebase.numer / timebase.denom / 1000000.0f;
}
