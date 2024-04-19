// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DEFAULT_SOURCE  // htonll, ntohll, htobe64, be64toh
#define _GNU_SOURCE      // strcasestr

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wchar.h>

#include "matoya.h"

void *MTY_AllocAligned(size_t size, size_t align)
{
	void *mem = NULL;
	int32_t e = posix_memalign(&mem, align, size);

	if (e != 0)
		MTY_LogFatal("'posix_memalign' failed with error %d", e);

	memset(mem, 0, size);

	return mem;
}

void MTY_FreeAligned(void *mem)
{
	free(mem);
}

int32_t MTY_Strcasecmp(const char *s0, const char *s1)
{
	return strcasecmp(s0, s1);
}

char *MTY_Strcasestr(const char *s0, const char *s1)
{
	return strcasestr(s0, s1);
}

char *MTY_Strtok(char *str, const char *delim, char **saveptr)
{
	return strtok_r(str, delim, saveptr);
}

__thread mbstate_t mbstate;

bool MTY_WideToMulti(const wchar_t *src, char *dst, size_t size)
{
	size_t n = wcsrtombs(dst, &src, size, &mbstate);

	if (n > 0 && n != (size_t) -1) {
		if (n == size) {
			MTY_Log("Conversion truncated");
			dst[size - 1] = L'\0';
		}
	} else {
		MTY_Log("'wcstombs' failed with errno %d", errno);
		memset(dst, 0, size);
		return false;
	}

	return true;
}

bool MTY_MultiToWide(const char *src, wchar_t *dst, uint32_t len)
{
	size_t n = mbsrtowcs(dst, &src, len, &mbstate);

	if (n > 0 && n != (size_t) -1) {
		if (n == len) {
			MTY_Log("Conversion truncated");
			dst[len - 1] = L'\0';
		}
	} else {
		MTY_Log("'mbstowcs' failed with errno %d", errno);
		memset(dst, 0, len * sizeof(wchar_t));
		return false;
	}

	return true;
}

uint16_t MTY_Swap16(uint16_t value)
{
	return __builtin_bswap16(value);
}

uint32_t MTY_Swap32(uint32_t value)
{
	return __builtin_bswap32(value);
}

uint64_t MTY_Swap64(uint64_t value)
{
	return __builtin_bswap64(value);
}

uint16_t MTY_SwapToBE16(uint16_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap16(value);
	#else
		return value;
	#endif
}

uint32_t MTY_SwapToBE32(uint32_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap32(value);
	#else
		return value;
	#endif
}

uint64_t MTY_SwapToBE64(uint64_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap64(value);
	#else
		return value;
	#endif
}

uint16_t MTY_SwapFromBE16(uint16_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap16(value);
	#else
		return value;
	#endif
}

uint32_t MTY_SwapFromBE32(uint32_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap32(value);
	#else
		return value;
	#endif
}

uint64_t MTY_SwapFromBE64(uint64_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap64(value);
	#else
		return value;
	#endif
}
