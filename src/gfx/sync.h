// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

struct sync {
	uint32_t interval;
	uint32_t rem;
};

static void sync_set_interval(struct sync *sync, uint32_t interval)
{
	sync->interval = interval;
	sync->rem = 0;
}

static uint32_t sync_next_interval(struct sync *sync)
{
	if (sync->interval == 0)
		return 0;

	uint32_t rem = sync->rem + sync->interval;
	uint32_t r = (uint32_t) (rem / 100.0);

	sync->rem = rem - r * 100;

	return r;
}
