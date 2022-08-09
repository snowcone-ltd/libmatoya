// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "dl/libcrypto.c"

struct MTY_AESGCM {
	EVP_CIPHER_CTX *enc;
	EVP_CIPHER_CTX *dec;
};

MTY_AESGCM *MTY_AESGCMCreate(const void *key)
{
	if (!libcrypto_global_init())
		return NULL;

	MTY_AESGCM *ctx = MTY_Alloc(1, sizeof(MTY_AESGCM));
	bool r = true;

	const EVP_CIPHER *cipher = EVP_aes_128_gcm();

	ctx->enc = EVP_CIPHER_CTX_new();
	if (!ctx->enc) {
		MTY_Log("'EVP_CIPHER_CTX_new' failed");
		r = false;
		goto except;
	}

	ctx->dec = EVP_CIPHER_CTX_new();
	if (!ctx->dec) {
		MTY_Log("'EVP_CIPHER_CTX_new' failed");
		r = false;
		goto except;
	}

	int32_t e = EVP_CipherInit_ex(ctx->enc, cipher, NULL, key, NULL, 1);
	if (e != 1) {
		MTY_Log("'EVP_CipherInit_ex' failed with error %d", e);
		r = false;
		goto except;
	}

	e = EVP_CipherInit_ex(ctx->dec, cipher, NULL, key, NULL, 0);
	if (e != 1) {
		MTY_Log("'EVP_CipherInit_ex' failed with error %d", e);
		r = false;
		goto except;
	}

	except:

	if (!r)
		MTY_AESGCMDestroy(&ctx);

	return ctx;
}

void MTY_AESGCMDestroy(MTY_AESGCM **aesgcm)
{
	if (!aesgcm || !*aesgcm)
		return;

	MTY_AESGCM *ctx = *aesgcm;

	if (ctx->dec)
		EVP_CIPHER_CTX_free(ctx->dec);

	if (ctx->enc)
		EVP_CIPHER_CTX_free(ctx->enc);

	MTY_Free(ctx);
	*aesgcm = NULL;
}

bool MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *nonce, const void *plainText, size_t size,
	void *tag, void *cipherText)
{
	int32_t e = EVP_CipherInit_ex(ctx->enc, NULL, NULL, NULL, nonce, 1);
	if (e != 1) {
		MTY_Log("'EVP_CipherInit' failed with error %d", e);
		return false;
	}

	int32_t len = 0;
	e = EVP_EncryptUpdate(ctx->enc, cipherText, &len, plainText, size);
	if (e != 1) {
		MTY_Log("'EVP_EncryptUpdate' failed with error %d", e);
		return false;
	}

	e = EVP_EncryptFinal_ex(ctx->enc, cipherText, &len);
	if (e != 1) {
		MTY_Log("'EVP_EncryptFinal_ex' failed with error %d", e);
		return false;
	}

	e = EVP_CIPHER_CTX_ctrl(ctx->enc, EVP_CTRL_GCM_GET_TAG, 16, tag);
	if (e != 1) {
		MTY_Log("'EVP_CIPHER_CTX_ctrl' failed with error %d", e);
		return false;
	}

	return true;
}

bool MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *nonce, const void *cipherText, size_t size,
	const void *tag, void *plainText)
{
	int32_t e = EVP_CipherInit_ex(ctx->dec, NULL, NULL, NULL, nonce, 0);
	if (e != 1) {
		MTY_Log("'EVP_CipherInit' failed with error %d", e);
		return false;
	}

	int32_t len = 0;
	e = EVP_DecryptUpdate(ctx->dec, plainText, &len, cipherText, size);
	if (e != 1) {
		MTY_Log("'EVP_DecryptUpdate' failed with error %d", e);
		return false;
	}

	e = EVP_CIPHER_CTX_ctrl(ctx->dec, EVP_CTRL_GCM_SET_TAG, 16, (void *) tag);
	if (e != 1) {
		MTY_Log("'EVP_CIPHER_CTX_ctrl' failed with error %d", e);
		return false;
	}

	e = EVP_DecryptFinal_ex(ctx->dec, plainText, &len);
	if (e != 1) {
		MTY_Log("'EVP_DecryptFinal_ex' failed with error %d", e);
		return false;
	}

	return true;
}
