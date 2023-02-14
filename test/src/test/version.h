// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

static bool version_main(void)
{
	uint32_t v = MTY_GetVersion();

	test_cmpi64("MTY_GetVersion", v == (MTY_VERSION_MAJOR << 8 | MTY_VERSION_MINOR),
		(uint64_t) v);

	return true;
}
