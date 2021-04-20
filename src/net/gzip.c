// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "gzip.h"

#include "miniz.h"

#define GZIP_CHUNK_SIZE (256 * 1024)

static size_t gzip_rejigger(const void *in, size_t inSize)
{
	// https://tools.ietf.org/html/rfc1952#page-6

	const uint8_t *in8 = in;
	size_t o = 10;

	// Minimum size check
	if (inSize < 11)
		return inSize;

	// GZIP format
	if (in8[0] != 0x1F && in8[1] != 0x8B)
		return inSize;

	// This specifies the deflate aglorithm (only option)
	if (in8[2] != 8)
		return inSize;

	// FEXTRA
	if (in8[3] & 0x4)
		o += 2;

	// FNAME
	if (in8[3] & 0x8)
		while (o < inSize && in8[o++] != '\0');

	// FCOMMENT
	if (in8[3] & 0x10)
		while (o < inSize && in8[o++] != '\0');

	// FHCRC
	if (in8[3] & 0x2)
		o += 2;

	return o;
}

void *mty_gzip_decompress(const void *in, size_t inSize, size_t *outSize)
{
	size_t o = gzip_rejigger(in, inSize);
	if (o >= inSize) {
		MTY_Log("Invalid gzip format");
		return NULL;
	}

	void *out = NULL;
	*outSize = 0;

	z_stream strm = {0};
	int32_t e = inflateInit2(&strm, -MZ_DEFAULT_WINDOW_BITS);
	if (e != Z_OK) {
		MTY_Log("'inflateInit2' failed with error %d", e);
		return NULL;
	}

	strm.avail_in = (uint32_t) (inSize - o);
	strm.next_in = (const uint8_t *) in + o;

	while (e == Z_OK) {
		out = MTY_Realloc(out, *outSize + GZIP_CHUNK_SIZE, 1);

		strm.avail_out = GZIP_CHUNK_SIZE;
		strm.next_out = (Bytef *) out + *outSize;

		e = inflate(&strm, Z_NO_FLUSH);

		*outSize += GZIP_CHUNK_SIZE - strm.avail_out;
	}

	if (e != Z_STREAM_END) {
		MTY_Log("'inflate' failed with error %d", e);

		MTY_Free(out);
		*outSize = 0;

		return NULL;
	}

	e = inflateEnd(&strm);
	if (e != Z_OK)
		MTY_Log("'inflateEnd' failed with error %d", e);

	return out;
}
