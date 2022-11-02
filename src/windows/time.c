// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <windows.h>
#include <timeapi.h>

#include "tlocal.h"

static TLOCAL bool TIME_FREQ_INIT;
static TLOCAL double TIME_FREQUENCY;

MTY_Time MTY_GetTime(void)
{
	LARGE_INTEGER ts;
	QueryPerformanceCounter(&ts);

	return ts.QuadPart;
}

double MTY_TimeDiff(MTY_Time begin, MTY_Time end)
{
	if (!TIME_FREQ_INIT) {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		TIME_FREQUENCY = frequency.QuadPart / 1000.0;
		TIME_FREQ_INIT = true;
	}

	return (end - begin) / TIME_FREQUENCY;
}

static HANDLE time_create_timer(BOOL manual)
{
	HANDLE timer = CreateWaitableTimer(NULL, manual, NULL);
	if (!timer)
		MTY_Log("'CreateWaitableTimer' faled with error 0x%X", GetLastError());

	return timer;
}

static bool time_wait(HANDLE timer, uint32_t timeout)
{
	LARGE_INTEGER ft = {.QuadPart = -10000 * (int32_t) timeout};

	if (SetWaitableTimerEx(timer, &ft, 0, NULL, NULL, NULL, 0)) {
		DWORD e = WaitForSingleObject(timer, INFINITE);
		if (e == WAIT_OBJECT_0)
			return true;

		MTY_Log("'WaitForSingleObject' returned %d", e);

	} else {
		MTY_Log("'SetWaitableTimer' failed with error 0x%X", GetLastError());
	}

	return false;
}

void MTY_Sleep(uint32_t timeout)
{
	// There is evidence that CreateWaitableTimer will produce higher resolution
	// waiting over Sleep

	HANDLE timer = time_create_timer(TRUE);
	if (!timer)
		return;

	time_wait(timer, timeout);

	CloseHandle(timer);
}

void MTY_PreciseSleep(double timeout, double spin)
{
	// Inspired by https://blog.bearcats.nl/accurate-sleep-function/

	MTY_Time start = MTY_GetTime();

	HANDLE timer = time_create_timer(FALSE);
	if (!timer)
		return;

	for (double ep = 0.0; ep < timeout - spin; ep = MTY_TimeDiff(start, MTY_GetTime()))
		if (!time_wait(timer, 1))
			break;

	CloseHandle(timer);

	while (MTY_TimeDiff(start, MTY_GetTime()) < timeout);
}

void MTY_SetTimerResolution(uint32_t res)
{
	MMRESULT e = timeBeginPeriod(res);
	if (e != TIMERR_NOERROR)
		MTY_Log("'timeBeginPeriod' returned %d", e);
}

void MTY_RevertTimerResolution(uint32_t res)
{
	MMRESULT e = timeEndPeriod(res);
	if (e != TIMERR_NOERROR)
		MTY_Log("'timeEndPeriod' returned %d", e);
}
