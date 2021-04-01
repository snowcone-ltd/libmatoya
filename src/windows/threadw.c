// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>

#include <windows.h>


// Thread

struct MTY_Thread {
	HANDLE thread;
	bool detach;
	MTY_ThreadFunc func;
	void *opaque;
	void *ret;
};

static DWORD WINAPI thread_func(LPVOID *lpParameter)
{
	MTY_Thread *ctx = (MTY_Thread *) lpParameter;

	ctx->ret = ctx->func(ctx->opaque);

	if (ctx->detach)
		MTY_Free(ctx);

	return 0;
}

static MTY_Thread *thread_create(MTY_ThreadFunc func, void *opaque, bool detach)
{
	MTY_Thread *ctx = MTY_Alloc(1, sizeof(MTY_Thread));
	ctx->func = func;
	ctx->opaque = opaque;
	ctx->detach = detach;

	HANDLE thread = CreateThread(NULL, 0, thread_func, ctx, 0, NULL);

	if (!thread)
		MTY_LogFatal("'CreateThread' failed with error 0x%X", GetLastError());

	if (detach) {
		if (!CloseHandle(thread))
			MTY_LogFatal("'CloseHandle' failed with error 0x%X", GetLastError());

		ctx = NULL;

	} else {
		ctx->thread = thread;
	}

	return ctx;
}

MTY_Thread *MTY_ThreadCreate(MTY_ThreadFunc func, void *opaque)
{
	return thread_create(func, opaque, false);
}

void *MTY_ThreadDestroy(MTY_Thread **thread)
{
	if (!thread || !*thread)
		return NULL;

	MTY_Thread *ctx = *thread;

	if (ctx->thread) {
		if (WaitForSingleObject(ctx->thread, INFINITE) == WAIT_FAILED)
			MTY_LogFatal("'WaitForSingleObject' failed with error 0x%X", GetLastError());

		if (!CloseHandle(ctx->thread))
			MTY_LogFatal("'CloseHandle' failed with error 0x%X", GetLastError());
	}

	void *ret = ctx->ret;

	MTY_Free(ctx);
	*thread = NULL;

	return ret;
}

void MTY_ThreadDetach(MTY_ThreadFunc func, void *opaque)
{
	thread_create(func, opaque, true);
}

int64_t MTY_ThreadGetID(MTY_Thread *ctx)
{
	return ctx ? GetThreadId(ctx->thread) : GetCurrentThreadId();
}


// Mutex

struct MTY_Mutex {
	CRITICAL_SECTION mutex;
};

MTY_Mutex *MTY_MutexCreate(void)
{
	MTY_Mutex *ctx = MTY_Alloc(1, sizeof(MTY_Mutex));

	InitializeCriticalSection(&ctx->mutex);

	return ctx;
}

void MTY_MutexDestroy(MTY_Mutex **mutex)
{
	if (!mutex || !*mutex)
		return;

	MTY_Mutex *ctx = *mutex;

	DeleteCriticalSection(&ctx->mutex);

	MTY_Free(ctx);
	*mutex = NULL;
}

void MTY_MutexLock(MTY_Mutex *ctx)
{
	EnterCriticalSection(&ctx->mutex);
}

bool MTY_MutexTryLock(MTY_Mutex *ctx)
{
	return TryEnterCriticalSection(&ctx->mutex);
}

void MTY_MutexUnlock(MTY_Mutex *ctx)
{
	LeaveCriticalSection(&ctx->mutex);
}


// Cond

struct MTY_Cond {
	CONDITION_VARIABLE cond;
};

MTY_Cond *MTY_CondCreate(void)
{
	MTY_Cond *ctx = MTY_Alloc(1, sizeof(MTY_Cond));

	InitializeConditionVariable(&ctx->cond);

	return ctx;
}

void MTY_CondDestroy(MTY_Cond **cond)
{
	if (!cond || !*cond)
		return;

	MTY_Cond *ctx = *cond;

	MTY_Free(ctx);
	*cond = NULL;
}

bool MTY_CondWait(MTY_Cond *ctx, MTY_Mutex *mutex, int32_t timeout)
{
	bool r = true;
	timeout = timeout < 0 ? INFINITE : timeout;

	BOOL success = SleepConditionVariableCS(&ctx->cond, &mutex->mutex, timeout);

	if (!success) {
		DWORD e = GetLastError();

		if (e == ERROR_TIMEOUT) {
			r = false;

		} else {
			MTY_LogFatal("'SleepConditionVariableCS' failed with error 0x%X", e);
		}
	}

	return r;
}

void MTY_CondSignal(MTY_Cond *ctx)
{
	WakeConditionVariable(&ctx->cond);
}

void MTY_CondSignalAll(MTY_Cond *ctx)
{
	WakeAllConditionVariable(&ctx->cond);
}


// Atomic

void MTY_Atomic32Set(MTY_Atomic32 *atomic, int32_t value)
{
	InterlockedExchange((volatile LONG *) &atomic->value, value);
}

void MTY_Atomic64Set(MTY_Atomic64 *atomic, int64_t value)
{
	InterlockedExchange64(&atomic->value, value);
}

int32_t MTY_Atomic32Get(MTY_Atomic32 *atomic)
{
	return InterlockedOr((volatile LONG *) &atomic->value, 0);
}

int64_t MTY_Atomic64Get(MTY_Atomic64 *atomic)
{
	return InterlockedOr64(&atomic->value, 0);
}

int32_t MTY_Atomic32Add(MTY_Atomic32 *atomic, int32_t value)
{
	return InterlockedAdd((volatile LONG *) &atomic->value, value);
}

int64_t MTY_Atomic64Add(MTY_Atomic64 *atomic, int64_t value)
{
	return InterlockedAdd64(&atomic->value, value);
}

bool MTY_Atomic32CAS(MTY_Atomic32 *atomic, int32_t oldValue, int32_t newValue)
{
	return InterlockedCompareExchange((volatile LONG *) &atomic->value, newValue, oldValue) == oldValue;
}

bool MTY_Atomic64CAS(MTY_Atomic64 *atomic, int64_t oldValue, int64_t newValue)
{
	return InterlockedCompareExchange64(&atomic->value, newValue, oldValue) == oldValue;
}
