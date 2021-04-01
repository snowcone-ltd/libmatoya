// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <windows.h>

typedef SRWLOCK mty_rwlock;

static void mty_rwlock_create(mty_rwlock *rwlock)
{
	InitializeSRWLock(rwlock);
}

static void mty_rwlock_reader(mty_rwlock *rwlock)
{
	AcquireSRWLockShared(rwlock);
}

static bool mty_rwlock_try_reader(mty_rwlock *rwlock)
{
	return TryAcquireSRWLockShared(rwlock);
}

static void mty_rwlock_writer(mty_rwlock *rwlock)
{
	AcquireSRWLockExclusive(rwlock);
}

static void mty_rwlock_unlock_reader(mty_rwlock *rwlock)
{
	ReleaseSRWLockShared(rwlock);
}

static void mty_rwlock_unlock_writer(mty_rwlock *rwlock)
{
	ReleaseSRWLockExclusive(rwlock);
}

static void mty_rwlock_destroy(mty_rwlock *rwlock)
{
}
