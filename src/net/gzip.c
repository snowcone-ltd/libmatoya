// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "gzip.h"

#include "miniz.h"

#define GZIP_CHUNK_SIZE (256 * 1024)

void *mty_gzip_decompress(const void *in, size_t inSize, size_t *outSize)
{
	void *out = NULL;
	*outSize = 0;

	z_stream strm = {0};
	int32_t e = inflateInit2(&strm, 16 + MAX_WBITS);
	if (e != Z_OK) {
		MTY_Log("'inflateInit2' failed with error %d", e);
		return NULL;
	}

	strm.avail_in = (uint32_t) inSize;
	strm.next_in = in;

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
