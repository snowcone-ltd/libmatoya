// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "dl/libcrypto.c"


// Hash

typedef unsigned char *(*CRYPTO_FUNC)(const unsigned char *d, size_t n, unsigned char *md);

static void crypto_hash(const EVP_MD *md, CRYPTO_FUNC hash, const void *input, size_t inputSize,
	const void *key, size_t keySize, void *output, size_t outputSize, size_t minSize)
{
	if (outputSize >= minSize) {
		if (key && keySize) {
			if (!HMAC(md, key, keySize, input, inputSize, output, NULL))
				MTY_Log("'HMAC' failed");
		} else {
			if (!hash(input, inputSize, output))
				MTY_Log("'hash' failed");
		}
	}
}

void MTY_CryptoHash(MTY_Algorithm algo, const void *input, size_t inputSize, const void *key,
	size_t keySize, void *output, size_t outputSize)
{
	if (!libcrypto_global_init())
		return;

	switch (algo) {
		case MTY_ALGORITHM_SHA1:
			crypto_hash(EVP_sha1(), SHA1, input, inputSize, key, keySize, output, outputSize, MTY_SHA1_SIZE);
			break;
		case MTY_ALGORITHM_SHA1_HEX: {
			uint8_t bytes[MTY_SHA1_SIZE];
			crypto_hash(EVP_sha1(), SHA1, input, inputSize, key, keySize, bytes, sizeof(bytes), MTY_SHA1_SIZE);
			MTY_BytesToHex(bytes, sizeof(bytes), output, outputSize);
			break;
		}
		case MTY_ALGORITHM_SHA256:
			crypto_hash(EVP_sha256(), SHA256, input, inputSize, key, keySize, output, outputSize, MTY_SHA256_SIZE);
			break;
		case MTY_ALGORITHM_SHA256_HEX: {
			uint8_t bytes[MTY_SHA256_SIZE];
			crypto_hash(EVP_sha256(), SHA256, input, inputSize, key, keySize, bytes, sizeof(bytes), MTY_SHA256_SIZE);
			MTY_BytesToHex(bytes, sizeof(bytes), output, outputSize);
			break;
		}
	}
}


// Random

void MTY_GetRandomBytes(void *buf, size_t size)
{
	if (!libcrypto_global_init())
		return;

	int32_t e = RAND_bytes(buf, size);
	if (e != 1)
		MTY_Log("'RAND_bytes' failed with error %d", e);
}


// Base64

void MTY_BytesToBase64(const void *bytes, size_t size, char *base64, size_t base64Size)
{
	size_t needed = (4 * size / 3 + 3) & ~3;

	if (base64Size <= needed) {
		MTY_Log("'base64Size' is too small");
		return;
	}

	if (!libcrypto_global_init())
		return;

	EVP_EncodeBlock((void *) base64, bytes, size);
}
