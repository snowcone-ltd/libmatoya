// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "thread.h"


// Thread

struct MTY_Thread {
	pthread_t thread;
	bool detach;
	MTY_ThreadFunc func;
	void *opaque;
	void *ret;
};

static void *thread_func(void *opaque)
{
	MTY_Thread *ctx = (MTY_Thread *) opaque;

	ctx->ret = ctx->func(ctx->opaque);

	if (ctx->detach)
		MTY_Free(ctx);

	return NULL;
}

static MTY_Thread *thread_create(MTY_ThreadFunc func, void *opaque, bool detach)
{
	MTY_Thread *ctx = MTY_Alloc(1, sizeof(MTY_Thread));
	ctx->func = func;
	ctx->opaque = opaque;
	ctx->detach = detach;

	pthread_t thread = 0;
	int32_t e = pthread_create(&thread, NULL, thread_func, ctx);

	if (e != 0)
		MTY_LogFatal("'pthread_create' failed with error %d", e);

	if (detach) {
		e = pthread_detach(thread);
		if (e != 0)
			MTY_LogFatal("'pthread_detach' failed with error %d", e);

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
		int32_t e = pthread_join(ctx->thread, NULL);
		if (e != 0)
			MTY_LogFatal("'pthread_join' failed with error %d", e);
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
	return (int64_t) (ctx ? ctx->thread : pthread_self());
}


// Mutex

struct MTY_Mutex {
	pthread_mutex_t mutex;
};

MTY_Mutex *MTY_MutexCreate(void)
{
	MTY_Mutex *ctx = MTY_Alloc(1, sizeof(MTY_Mutex));

	int32_t e = pthread_mutex_init(&ctx->mutex, NULL);
	if (e != 0)
		MTY_LogFatal("'pthread_mutex_init' failed with error %d", e);

	return ctx;
}

void MTY_MutexDestroy(MTY_Mutex **mutex)
{
	if (!mutex || !*mutex)
		return;

	MTY_Mutex *ctx = *mutex;

	int32_t e = pthread_mutex_destroy(&ctx->mutex);
	if (e != 0)
		MTY_LogFatal("'pthread_mutex_destroy' failed with error %d", e);

	MTY_Free(ctx);
	*mutex = NULL;
}

void MTY_MutexLock(MTY_Mutex *ctx)
{
	int32_t e = pthread_mutex_lock(&ctx->mutex);
	if (e != 0)
		MTY_LogFatal("'pthread_mutex_lock' failed with error %d", e);
}

bool MTY_MutexTryLock(MTY_Mutex *ctx)
{
	int32_t e = pthread_mutex_trylock(&ctx->mutex);
	if (e != 0 && e != EBUSY)
		MTY_LogFatal("'pthread_mutex_trylock' failed with error %d", e);

	return e == 0;
}

void MTY_MutexUnlock(MTY_Mutex *ctx)
{
	int32_t e = pthread_mutex_unlock(&ctx->mutex);
	if (e != 0)
		MTY_LogFatal("'pthread_mutex_unlock' failed with error %d", e);
}


// Cond

struct MTY_Cond {
	pthread_cond_t cond;
};

MTY_Cond *MTY_CondCreate(void)
{
	MTY_Cond *ctx = MTY_Alloc(1, sizeof(MTY_Cond));

	int32_t e = pthread_cond_init(&ctx->cond, NULL);
	if (e != 0)
		MTY_LogFatal("'pthread_cond_init' failed with error %d", e);

	return ctx;
}

void MTY_CondDestroy(MTY_Cond **cond)
{
	if (!cond || !*cond)
		return;

	MTY_Cond *ctx = *cond;

	int32_t e = pthread_cond_destroy(&ctx->cond);
	if (e != 0)
		MTY_LogFatal("'pthread_cond_destroy' failed with error %d", e);

	MTY_Free(ctx);
	*cond = NULL;
}

bool MTY_CondWait(MTY_Cond *ctx, MTY_Mutex *mutex, int32_t timeout)
{
	// Use pthread_cond_timedwait
	if (timeout >= 0) {
		struct timespec ts = {0};

		if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
			MTY_LogFatal("'clock_gettime' failed with errno %d", errno);

		ts.tv_sec += timeout / 1000;
		ts.tv_nsec += (timeout % 1000) * 1000 * 1000;
		ts.tv_sec += ts.tv_nsec / 1000000000;
		ts.tv_nsec %= 1000000000;

		int32_t e = pthread_cond_timedwait(&ctx->cond, &mutex->mutex, &ts);
		if (e == ETIMEDOUT) {
			return false;

		} else if (e != 0) {
			MTY_LogFatal("'pthread_cond_timedwait' failed with error %d", e);
		}

	// Use pthread_cond_wait
	} else {
		int32_t e = pthread_cond_wait(&ctx->cond, &mutex->mutex);
		if (e != 0)
			MTY_LogFatal("'pthread_cond_wait' failed with error %d", e);
	}

	return true;
}

void MTY_CondSignal(MTY_Cond *ctx)
{
	int32_t e = pthread_cond_signal(&ctx->cond);
	if (e != 0)
		MTY_LogFatal("'pthread_cond_signal' failed with error %d", e);
}

void MTY_CondSignalAll(MTY_Cond *ctx)
{
	int32_t e = pthread_cond_broadcast(&ctx->cond);
	if (e != 0)
		MTY_LogFatal("'pthread_cond_broadcast' failed with error %d", e);
}


// Atomic

// XXX Android will complain about the 64-bit atomics on 32-bit platforms,
// there is probably a performance penalty but not critical enough to care

void MTY_Atomic32Set(MTY_Atomic32 *atomic, int32_t value)
{
	__atomic_store(&atomic->value, &value, __ATOMIC_SEQ_CST);
}

void MTY_Atomic64Set(MTY_Atomic64 *atomic, int64_t value)
{
	__atomic_store(&atomic->value, &value, __ATOMIC_SEQ_CST);
}

int32_t MTY_Atomic32Get(MTY_Atomic32 *atomic)
{
	int32_t r;

	__atomic_load(&atomic->value, &r, __ATOMIC_SEQ_CST);

	return r;
}

int64_t MTY_Atomic64Get(MTY_Atomic64 *atomic)
{
	int64_t r;

	__atomic_load(&atomic->value, &r, __ATOMIC_SEQ_CST);

	return r;
}

int32_t MTY_Atomic32Add(MTY_Atomic32 *atomic, int32_t value)
{
	return __atomic_add_fetch(&atomic->value, value, __ATOMIC_SEQ_CST);
}

int64_t MTY_Atomic64Add(MTY_Atomic64 *atomic, int64_t value)
{
	return __atomic_add_fetch(&atomic->value, value, __ATOMIC_SEQ_CST);
}

bool MTY_Atomic32CAS(MTY_Atomic32 *atomic, int32_t oldValue, int32_t newValue)
{
	return __atomic_compare_exchange_n(&atomic->value, &oldValue, newValue, false,
		__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

bool MTY_Atomic64CAS(MTY_Atomic64 *atomic, int64_t oldValue, int64_t newValue)
{
	return __atomic_compare_exchange_n(&atomic->value, &oldValue, newValue, false,
		__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
