// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

// DTLS 1.2 or TLS 1.2
#define TLS_IS_SUPPORTED(d, size) \
	(size > 2 && ((d[1] == 0xFE && d[2] == 0xFD) || (d[1] == 0x03 && d[2] == 0x03)))

bool MTY_IsTLSHandshake(const void *buf, size_t size)
{
	const uint8_t *d = buf;

	// Change Cipher Spec, Handshake
	return TLS_IS_SUPPORTED(d, size) && (d[0] == 0x14 || d[0] == 0x16);
}

bool MTY_IsTLSApplicationData(const void *buf, size_t size)
{
	const uint8_t *d = buf;

	// Application Data
	return TLS_IS_SUPPORTED(d, size) && d[0] == 0x17;
}
