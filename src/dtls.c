// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

// DTLS 1.2
#define DTLS_IS_SUPPORTED(d, size) \
	(size > 2 && d[1] == 0xFE && d[2] == 0xFD)

bool MTY_IsDTLSHandshake(const void *buf, size_t size)
{
	const uint8_t *d = buf;

	// Change Cipher Spec, Handshake
	return DTLS_IS_SUPPORTED(d, size) && (d[0] == 0x14 || d[0] == 0x16);
}

bool MTY_IsDTLSApplicationData(const void *buf, size_t size)
{
	const uint8_t *d = buf;

	// Application Data
	return DTLS_IS_SUPPORTED(d, size) && d[0] == 0x17;
}
