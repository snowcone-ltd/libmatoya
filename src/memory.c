// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wchar.h>

#include "tlocal.h"

static volatile void *(*MEMORY_MEMSET)(void *s, int c, size_t n) = (void *) memset;

void MTY_SecureZero(void *mem, size_t size)
{
	// This prevents the compiler from optimizing memset away
	if (mem)
		MEMORY_MEMSET(mem, 0, size);
}

void *MTY_Alloc(size_t len, size_t size)
{
	void *mem = calloc(len, size);

	if (!mem)
		MTY_LogFatal("'calloc' failed with errno %d", errno);

	return mem;
}

void MTY_Free(void *mem)
{
	free(mem);
}

void MTY_SecureFree(void *mem, size_t size)
{
	MTY_SecureZero(mem, size);
	MTY_Free(mem);
}

void *MTY_Realloc(void *mem, size_t len, size_t size)
{
	size_t total = size * len;

	void *new_mem = realloc(mem, total);

	if (!new_mem && total > 0)
		MTY_LogFatal("'realloc' failed with errno %d", errno);

	return new_mem;
}

void *MTY_Dup(const void *mem, size_t size)
{
	void *dup = MTY_Alloc(size, 1);
	memcpy(dup, mem, size);

	return dup;
}

char *MTY_Strdup(const char *str)
{
	return MTY_Dup(str, strlen(str) + 1);
}

void MTY_Strcat(char *dst, size_t size, const char *src)
{
	size_t dst_len = strlen(dst);
	size_t src_len = strlen(src);

	if (dst_len + src_len + 1 > size)
		return;

	memcpy(dst + dst_len, src, src_len + 1);
}

char *MTY_VsprintfD(const char *fmt, va_list args)
{
	// va_list can be exhausted each time it is referenced
	// by a ...v style function. Since we use it twice, make a copy

	va_list args_copy;
	va_copy(args_copy, args);

	size_t size = vsnprintf(NULL, 0, fmt, args_copy) + 1;

	va_end(args_copy);

	char *str = MTY_Alloc(size, 1);
	vsnprintf(str, size, fmt, args);

	return str;
}

char *MTY_SprintfD(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char *str = MTY_VsprintfD(fmt, args);

	va_end(args);

	return str;
}

const char *MTY_SprintfDL(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char *str = MTY_VsprintfD(fmt, args);

	va_end(args);

	char *local = mty_tlocal_strcpy(str);
	MTY_Free(str);

	return local;
}


// Stable qsort

struct element {
	MTY_CompareFunc compare;
	const void *orig;
};

static int32_t sort_compare(const void *a, const void *b)
{
	const struct element *element_a = a;
	const struct element *element_b = b;

	int32_t r = element_a->compare(element_a->orig, element_b->orig);

	// If zero is returned from original comare, use the memory address as the tie breaker
	return r != 0 ? r : (int32_t) ((uint8_t *) element_a->orig - (uint8_t *) element_b->orig);
}

void MTY_Sort(void *buf, size_t len, size_t size, MTY_CompareFunc func)
{
	// Temporary copy of the array for wrapping
	uint8_t *tmp = MTY_Alloc(len, size);
	memcpy(tmp, buf, len * size);

	// Wrap the array elements in a struct now with 'orig' memory addresses in ascending order
	struct element *wrapped = MTY_Alloc(len, sizeof(struct element));

	for (size_t x = 0; x < len; x++) {
		wrapped[x].compare = func;
		wrapped[x].orig = tmp + x * size;
	}

	// Perform qsort using the original compare function, falling back to memory address comparison
	qsort(wrapped, len, sizeof(struct element), sort_compare);

	// Copy the reordered elements back to the array
	for (size_t x = 0; x < len; x++)
		memcpy((uint8_t *) buf + x * size, wrapped[x].orig, size);

	MTY_Free(tmp);
	MTY_Free(wrapped);
}


// UTF-8 conversion

char *MTY_WideToMultiD(const wchar_t *src)
{
	if (!src)
		return NULL;

	// UTF8 may be up to 4 bytes per character
	size_t len = (wcslen(src) + 1) * 4;
	char *dst = MTY_Alloc(len, 1);

	MTY_WideToMulti(src, dst, len);

	return dst;
}

const char *MTY_WideToMultiDL(const wchar_t *src)
{
	if (!src)
		return NULL;

	// UTF8 may be up to 4 bytes per character
	size_t len = (wcslen(src) + 1) * 4;
	char *dst = mty_tlocal(len);

	MTY_WideToMulti(src, dst, len);

	return dst;
}

wchar_t *MTY_MultiToWideD(const char *src)
{
	if (!src)
		return NULL;

	uint32_t len = (uint32_t) strlen(src) + 1;
	wchar_t *dst = MTY_Alloc(len, sizeof(wchar_t));

	MTY_MultiToWide(src, dst, len);

	return dst;
}

const wchar_t *MTY_MultiToWideDL(const char *src)
{
	if (!src)
		return NULL;

	uint32_t len = (uint32_t) strlen(src) + 1;
	wchar_t *dst = mty_tlocal(len * sizeof(wchar_t));

	MTY_MultiToWide(src, dst, len);

	return dst;
}
