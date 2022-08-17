// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
	#include <emmintrin.h>

#elif defined(__aarch64__)
	#define STBI_NEON
	#include <arm_neon.h>
#endif

#define STBI_MALLOC(size)        MTY_Alloc(size, 1)
#define STBI_REALLOC(ptr, size)  MTY_Realloc(ptr, size, 1)
#define STBI_FREE(ptr)           MTY_Free(ptr)
#define STBI_ASSERT(x)

#include "stb_image.h"

void *MTY_CompressImage(MTY_ImageCompression method, const void *input, uint32_t width,
	uint32_t height, size_t *outputSize)
{
	return NULL;
}

void *MTY_DecompressImage(const void *input, size_t size, uint32_t *width, uint32_t *height)
{
	int32_t channels = 0;
	void *output = stbi_load_from_memory(input, (int32_t) size, (int32_t *) width, (int32_t *) height, &channels, 4);

	if (!output) {
		if (stbi__g_failure_reason)
			MTY_Log("%s", stbi__g_failure_reason);

		MTY_Log("'stbi_load_from_memory' failed");
	}

	return output;
}

void MTY_DecompressImageAsync(const void *input, size_t size, MTY_ImageFunc func, void *opaque)
{
	uint32_t w = 0;
	uint32_t h = 0;

	void *image = MTY_DecompressImage(input, size, &w, &h);

	func(image, w, h, opaque);
}

void *MTY_GetProgramIcon(const char *path, uint32_t *width, uint32_t *height)
{
	return NULL;
}
