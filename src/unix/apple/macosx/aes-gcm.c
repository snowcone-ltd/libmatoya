// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>

#include <CommonCrypto/CommonCryptor.h>


// Private interface -- These are not compaible with the App Store!

enum {
	kCCModeGCM = 11,
};

CCCryptorStatus CCCryptorGCMReset(CCCryptorRef cryptorRef);
CCCryptorStatus CCCryptorGCMAddIV(CCCryptorRef cryptorRef, const void *iv, size_t ivLen);
CCCryptorStatus CCCryptorGCMEncrypt(CCCryptorRef cryptorRef, const void *dataIn, size_t dataInLength, void *dataOut);
CCCryptorStatus CCCryptorGCMDecrypt(CCCryptorRef cryptorRef, const void *dataIn, size_t dataInLength, void *dataOut);
CCCryptorStatus CCCryptorGCMFinal(CCCryptorRef cryptorRef, void *tagOut, size_t *tagLength);


// AESGCM

struct MTY_AESGCM {
	CCCryptorRef dec;
	CCCryptorRef enc;
};

MTY_AESGCM *MTY_AESGCMCreate(const void *key, size_t keySize)
{
	if (keySize != 16 && keySize != 32)
		return NULL;

	MTY_AESGCM *ctx = MTY_Alloc(1, sizeof(MTY_AESGCM));

	CCCryptorStatus e = CCCryptorCreateWithMode(kCCEncrypt, kCCModeGCM, kCCAlgorithmAES,
		0, NULL, key, keySize, NULL, 0, 0, 0, &ctx->enc);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptoCreateWithMode' failed with error %d", e);
		goto except;
	}

	e = CCCryptorCreateWithMode(kCCDecrypt, kCCModeGCM, kCCAlgorithmAES,
		0, NULL, key, keySize, NULL, 0, 0, 0, &ctx->dec);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptoCreateWithMode' failed with error %d", e);
		goto except;
	}

	except:

	if (e != kCCSuccess)
		MTY_AESGCMDestroy(&ctx);

	return ctx;
}

void MTY_AESGCMDestroy(MTY_AESGCM **aesgcm)
{
	if (!aesgcm || !*aesgcm)
		return;

	MTY_AESGCM *ctx = *aesgcm;

	if (ctx->enc)
		CCCryptorRelease(ctx->enc);

	if (ctx->dec)
		CCCryptorRelease(ctx->dec);

	MTY_Free(ctx);
	*aesgcm = NULL;
}

bool MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *nonce, const void *plainText, size_t size,
	void *tag, void *cipherText)
{
	CCCryptorStatus e = CCCryptorGCMReset(ctx->enc);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorReset' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMAddIV(ctx->enc, nonce, 12);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMAddIV' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMEncrypt(ctx->enc, plainText, size, cipherText);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMEncrypt' failed with error %d", e);
		return false;
	}

	size_t hash_size = 16;
	e = CCCryptorGCMFinal(ctx->enc, tag, &hash_size);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMFinal' failed with error %d", e);
		return false;
	}

	return true;
}

bool MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *nonce, const void *cipherText, size_t size,
	const void *tag, void *plainText)
{
	CCCryptorStatus e = CCCryptorGCMReset(ctx->dec);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorReset' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMAddIV(ctx->dec, nonce, 12);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMAddIV' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMDecrypt(ctx->dec, cipherText, size, plainText);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMDecrypt' failed with error %d", e);
		return false;
	}

	uint8_t hashf[32];
	size_t hashf_size = 32;
	e = CCCryptorGCMFinal(ctx->dec, hashf, &hashf_size);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMFinal' failed with error %d", e);
		return false;
	}

	if (memcmp(tag, hashf, MTY_MIN(hashf_size, 16))) {
		MTY_Log("Authentication tag mismatch");
		return false;
	}

	return true;
}
