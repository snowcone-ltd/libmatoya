// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <errno.h>

#include <pthread.h>

#include "rwlockattr.h"

typedef pthread_rwlock_t mty_rwlock;

static void mty_rwlock_create(mty_rwlock *rwlock)
{
	pthread_rwlockattr_t attr;

	int32_t e = pthread_rwlockattr_init(&attr);
	if (e != 0)
		MTY_LogFatal("'pthread_rwlockattr_init' failed with error %d", e);

	// Wrapper for RWLOCK_PREFER_WRITER
	mty_rwlockattr_set(&attr);

	e = pthread_rwlock_init(rwlock, &attr);
	if (e != 0)
		MTY_LogFatal("'pthread_rwlock_init' failed with error %d", e);

	e = pthread_rwlockattr_destroy(&attr);
	if (e != 0)
		MTY_LogFatal("'pthread_rwlockattr_destroy' failed with error %d", e);
}

static void mty_rwlock_reader(mty_rwlock *rwlock)
{
	int32_t e = pthread_rwlock_rdlock(rwlock);
	if (e != 0)
		MTY_LogFatal("'pthread_rwlock_rdlock' failed with error %d", e);
}

static bool mty_rwlock_try_reader(mty_rwlock *rwlock)
{
	int32_t e = pthread_rwlock_tryrdlock(rwlock);
	if (e != 0 && e != EBUSY)
		MTY_LogFatal("'pthread_rwlock_tryrdlock' failed with error %d", e);

	return e == 0;
}

static void mty_rwlock_writer(mty_rwlock *rwlock)
{
	int32_t e = pthread_rwlock_wrlock(rwlock);
	if (e != 0)
		MTY_LogFatal("'pthread_rwlock_wrlock' failed with error %d", e);
}

static void mty_rwlock_unlock_reader(mty_rwlock *rwlock)
{
	int32_t e = pthread_rwlock_unlock(rwlock);
	if (e != 0)
		MTY_Log("'pthread_rwlock_unlock' failed with error %d", e);
}

static void mty_rwlock_unlock_writer(mty_rwlock *rwlock)
{
	mty_rwlock_unlock_reader(rwlock);
}

static void mty_rwlock_destroy(mty_rwlock *rwlock)
{
	int32_t e = pthread_rwlock_destroy(rwlock);
	if (e != 0)
		MTY_LogFatal("'pthread_rwlock_destroy' failed with error %d", e);
}
