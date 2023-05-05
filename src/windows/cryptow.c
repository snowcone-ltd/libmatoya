// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>

#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>


// Hash

static void crypto_hash(const void *input, size_t inputSize, const void *key, size_t keySize,
	const wchar_t *alg_id, void *output, size_t outputSize)
{
	BCRYPT_ALG_HANDLE ahandle = NULL;
	BCRYPT_HASH_HANDLE hhandle = NULL;

	NTSTATUS e = BCryptOpenAlgorithmProvider(&ahandle, alg_id, NULL, key ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptOpenAlgorithmProvider' failed with error 0x%X", e);
		goto except;
	}

	e = BCryptCreateHash(ahandle, &hhandle, NULL, 0, (UCHAR *) key, (ULONG) keySize, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptCreateHash' failed with error 0x%X", e);
		goto except;
	}

	e = BCryptHashData(hhandle, (UCHAR *) input, (ULONG) inputSize, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptHashData' failed with error 0x%X", e);
		goto except;
	}

	DWORD size = 0;
	ULONG written = 0;
	e = BCryptGetProperty(hhandle, BCRYPT_HASH_LENGTH, (UCHAR *) &size, sizeof(DWORD), &written, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptGetProperty' failed with error 0x%X", e);
		goto except;
	}

	if (size > outputSize) {
		MTY_Log("'outputSize' must be at least %u", size);
		goto except;
	}

	e = BCryptFinishHash(hhandle, output, size, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptFinishHash' failed with error 0x%X", e);
		goto except;
	}

	except:

	if (hhandle)
		BCryptDestroyHash(hhandle);

	if (ahandle)
		BCryptCloseAlgorithmProvider(ahandle, 0);
}

void MTY_CryptoHash(MTY_Algorithm algo, const void *input, size_t inputSize, const void *key,
	size_t keySize, void *output, size_t outputSize)
{
	switch (algo) {
		case MTY_ALGORITHM_SHA1:
			crypto_hash(input, inputSize, key, keySize, BCRYPT_SHA1_ALGORITHM, output, outputSize);
			break;
		case MTY_ALGORITHM_SHA1_HEX: {
			uint8_t bytes[MTY_SHA1_SIZE];
			crypto_hash(input, inputSize, key, keySize, BCRYPT_SHA1_ALGORITHM, bytes, sizeof(bytes));
			MTY_BytesToHex(bytes, sizeof(bytes), output, outputSize);
			break;
		}
		case MTY_ALGORITHM_SHA256:
			crypto_hash(input, inputSize, key, keySize, BCRYPT_SHA256_ALGORITHM, output, outputSize);
			break;
		case MTY_ALGORITHM_SHA256_HEX: {
			uint8_t bytes[MTY_SHA256_SIZE];
			crypto_hash(input, inputSize, key, keySize, BCRYPT_SHA256_ALGORITHM, bytes, sizeof(bytes));
			MTY_BytesToHex(bytes, sizeof(bytes), output, outputSize);
			break;
		}
	}
}


// Random

void MTY_GetRandomBytes(void *buf, size_t size)
{
	NTSTATUS e = BCryptGenRandom(NULL, buf, (ULONG) size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	if (e != STATUS_SUCCESS)
		MTY_Log("'BCryptGenRandom' failed with error 0x%X", e);
}


// Base64

void MTY_BytesToBase64(const void *bytes, size_t size, char *base64, size_t base64Size)
{
	DWORD dsize = (DWORD) base64Size;

	if (!CryptBinaryToStringA(bytes, (DWORD) size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64, &dsize))
		MTY_Log("'CryptBinaryToString' failed with error 0x%X", GetLastError());
}
