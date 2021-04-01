// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <limits.h>
#include <string.h>

static const char CRYPTO_HEX[16] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
};

static const char CRYPTO_HEX_REVERSE[CHAR_MAX] = {
	['0'] = 0,   ['1'] = 1,   ['2'] = 2,   ['3'] = 3,
	['4'] = 4,   ['5'] = 5,   ['6'] = 6,   ['7'] = 7,
	['8'] = 8,   ['9'] = 9,   ['a'] = 0xa, ['b'] = 0xb,
	['c'] = 0xc, ['d'] = 0xd, ['e'] = 0xe, ['f'] = 0xf,
	['A'] = 0xa, ['B'] = 0xb, ['C'] = 0xc, ['D'] = 0xd,
	['E'] = 0xe, ['F'] = 0xf,
};

static const char CRYPTO_B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

uint32_t MTY_CRC32(uint32_t crc, const void *buf, size_t size)
{
	uint32_t table[0x100];

	for (uint32_t x = 0; x < 0x100; x++) {
		uint32_t r = x;

		for (uint8_t y = 0; y < 8; y++)
			r = (r & 1 ? 0 : 0xEDB88320) ^ r >> 1;

		table[x] = r ^ 0xFF000000;
	}

	for (size_t x = 0; x < size; x++)
		crc = table[(uint8_t) crc ^ ((const uint8_t *) buf)[x]] ^ crc >> 8;

	return crc;
}

uint32_t MTY_DJB2(const char *str)
{
	uint32_t hash = 5381;

	while (*str)
		hash = ((hash << 5) + hash) + *str++;

	return hash;
}

void MTY_BytesToHex(const void *bytes, size_t size, char *hex, size_t hexSize)
{
	if (hexSize == 0)
		return;

	size_t offset = 0;
	for (size_t x = 0; x < size; x++, offset += 2) {
		if (offset + 2 >= hexSize) {
			MTY_Log("'hex' not large enough, truncated");
			break;
		}

		uint8_t byte = ((uint8_t *) bytes)[x];
		hex[offset] = CRYPTO_HEX[byte >> 4];
		hex[offset + 1] = CRYPTO_HEX[byte & 0x0F];
	}

	hex[offset] = '\0';
}

void MTY_HexToBytes(const char *hex, void *bytes, size_t size)
{
	for (size_t x = 0; x < strlen(hex); x++) {
		int8_t b = (int8_t) hex[x];

		if (b < 0)
			continue;

		size_t index = x / 2;

		if (index >= size) {
			MTY_Log("'bytes' not large enough, truncated");
			break;
		}

		uint8_t val = CRYPTO_HEX_REVERSE[b];

		if ((x & 0x1) == 0) {
			((uint8_t *) bytes)[index] = val << 4;

		} else {
			((uint8_t *) bytes)[index] |= val;
		}
	}
}

void MTY_BytesToBase64(const void *bytes, size_t size, char *base64, size_t base64Size)
{
	if (base64Size == 0)
		return;

	uint8_t *buf = (uint8_t *) bytes;
	memset(base64, 0, base64Size);

	for (size_t x = 0, p = 0; x < size; x += 3) {
		if (p + 3 >= base64Size) {
			MTY_Log("'base64' not large enough, truncated");
			break;
		}

		uint8_t b = buf[x];
		uint8_t shift = (b >> 2) & 0x3F;
		uint8_t rem = (b << 4) & 0x30;

		base64[p++] = CRYPTO_B64[shift];

		if (x + 1 < size) {
			b = buf[x + 1];
			shift = (b >> 4) & 0x0F;
			base64[p++] = CRYPTO_B64[rem | shift];
			rem = (b << 2) & 0x3C;

		} else {
			base64[p + 2] = '=';
		}

		if (x + 2 < size) {
			b = buf[x + 2];
			shift = (b >> 6) & 0x03;
			base64[p++] = CRYPTO_B64[rem | shift];
			rem = (b << 0) & 0x3F;

		} else {
			base64[p + 1] = '=';
		}

		base64[p++] = CRYPTO_B64[rem];
	}
}

bool MTY_CryptoHashFile(MTY_Algorithm algo, const char *path, const void *key, size_t keySize,
	void *output, size_t outputSize)
{
	size_t size = 0;
	void *input = MTY_ReadFile(path, &size);

	if (input) {
		MTY_CryptoHash(algo, input, size, key, keySize, output, outputSize);
		MTY_Free(input);

		return true;
	}

	return false;
}

uint32_t MTY_GetRandomUInt(uint32_t minVal, uint32_t maxVal)
{
	if (minVal >= maxVal) {
		MTY_Log("'minVal' can not be >= maxVal");
		return minVal;
	}

	uint32_t val = 0;
	MTY_GetRandomBytes(&val, sizeof(uint32_t));

	return val % (maxVal - minVal) + minVal;
}
