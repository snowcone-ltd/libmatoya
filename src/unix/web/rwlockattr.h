// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "web.h"

#define mty_rwlockattr_set(attr) (void) (attr)

static void thread_run(MTY_ThreadLoop loop, void *opaque)
{
	web_thread_run(loop, opaque);
}
