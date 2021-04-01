// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>

#include <winsock2.h>

void *MTY_AllocAligned(size_t size, size_t align)
{
	void *mem = _aligned_malloc(size, align);

	if (!mem)
		MTY_LogFatal("'_aligned_malloc' failed");

	memset(mem, 0, size);

	return mem;
}

void MTY_FreeAligned(void *mem)
{
	_aligned_free(mem);
}

int32_t MTY_Strcasecmp(const char *s0, const char *s1)
{
	return _stricmp(s0, s1);
}

char *MTY_Strtok(char *str, const char *delim, char **saveptr)
{
	return strtok_s(str, delim, saveptr);
}

bool MTY_WideToMulti(const wchar_t *src, char *dst, size_t size)
{
	int32_t n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, src, -1, dst,
		(int32_t) size, NULL, NULL);

	// WideCharToMultiByte will null terminate the output if -1 is supplied for the 'src' length,
	// otherwise it will fail
	if (n <= 0) {
		DWORD e = GetLastError();
		if (e != ERROR_NO_UNICODE_TRANSLATION)
			MTY_Log("'WideCharToMultiByte' failed with error 0x%X", e);

		// Fall back to current code page translation
		snprintf(dst, size, "%ls", src);
		return false;
	}

	return true;
}

bool MTY_MultiToWide(const char *src, wchar_t *dst, uint32_t len)
{
	int32_t n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, dst,
		(int32_t) len);

	// MutliByteToWideChar will null terminate the output if -1 is supplied for the 'src' length,
	// otherwise it will fail
	if (n <= 0) {
		DWORD e = GetLastError();
		if (e != ERROR_NO_UNICODE_TRANSLATION)
			MTY_Log("'MultiByteToWideChar' failed with error 0x%X", e);

		// Fall back to current code page translation
		_snwprintf_s(dst, len, _TRUNCATE, L"%hs", src);
		return false;
	}

	return true;
}

uint16_t MTY_Swap16(uint16_t value)
{
	return _byteswap_ushort(value);
}

uint32_t MTY_Swap32(uint32_t value)
{
	return _byteswap_ulong(value);
}

uint64_t MTY_Swap64(uint64_t value)
{
	return _byteswap_uint64(value);
}

uint16_t MTY_SwapToBE16(uint16_t value)
{
	return htons(value);
}

uint32_t MTY_SwapToBE32(uint32_t value)
{
	return htonl(value);
}

uint64_t MTY_SwapToBE64(uint64_t value)
{
	return htonll(value);
}

uint16_t MTY_SwapFromBE16(uint16_t value)
{
	return ntohs(value);
}

uint32_t MTY_SwapFromBE32(uint32_t value)
{
	return ntohl(value);
}

uint64_t MTY_SwapFromBE64(uint64_t value)
{
	return ntohll(value);
}
