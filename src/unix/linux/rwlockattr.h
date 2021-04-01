// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdlib.h>

#include <pthread.h>

static void mty_rwlockattr_set(pthread_rwlockattr_t *attr)
{
	int32_t e = pthread_rwlockattr_setkind_np(attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
	if (e != 0)
		MTY_LogFatal("'pthread_rwlockattr_setkind_np' failed with error %d", e);
}
