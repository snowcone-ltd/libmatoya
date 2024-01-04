// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

void *MTY_Compress(const void *input, size_t inputSize, size_t *outputSize)
{
	*outputSize = inputSize;

	return MTY_Dup(input, inputSize);
}

void *MTY_Decompress(const void *input, size_t inputSize, size_t *outputSize)
{
	*outputSize = inputSize;

	return MTY_Dup(input, inputSize);
}
