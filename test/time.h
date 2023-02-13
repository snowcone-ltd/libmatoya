// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

static bool time_main(void)
{
	MTY_SetTimerResolution(1);

	MTY_Time ts = MTY_GetTime();
	test_cmpi64("MTY_GetTime", ts > 0, ts);

	MTY_Sleep(100);
	test_cmp("MTY_Sleep", 100);

	double diff = MTY_TimeDiff(ts, MTY_GetTime());
	test_cmpf("MTY_TimeDiff", diff >= 99.0f && diff <= 115.0f, diff);

	MTY_RevertTimerResolution(1);

	return true;
}
