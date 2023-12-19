// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <windows.h>
#include <compressapi.h>

void *MTY_Compress(const void *input, size_t inputSize, size_t *outputSize)
{
	COMPRESSOR_HANDLE h = NULL;

	if (!CreateCompressor(COMPRESS_ALGORITHM_MSZIP, NULL, &h))
		return NULL;

	BOOL success = Compress(h, input, inputSize, NULL, 0, outputSize);
	if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return NULL;

	void *output = MTY_Alloc(*outputSize, 1);

	if (!Compress(h, input, inputSize, output, *outputSize, outputSize)) {
		MTY_Free(output);
		output = NULL;
	}

	CloseCompressor(h);

	return output;
}

void *MTY_Decompress(const void *input, size_t inputSize, size_t *outputSize)
{
	DECOMPRESSOR_HANDLE h = NULL;

	if (!CreateDecompressor(COMPRESS_ALGORITHM_MSZIP, NULL, &h))
		return NULL;

	BOOL success = Decompress(h, input, inputSize, NULL, 0, outputSize);
	if (!success && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return NULL;

	void *output = MTY_Alloc(*outputSize, 1);

	if (!Decompress(h, input, inputSize, output, *outputSize, outputSize)) {
		MTY_Free(output);
		output = NULL;
	}

	CloseDecompressor(h);

	return output;
}
