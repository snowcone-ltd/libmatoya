// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <bcrypt.h>

struct MTY_AESGCM {
	BCRYPT_ALG_HANDLE ahandle;
	BCRYPT_KEY_HANDLE khandle;
};

MTY_AESGCM *MTY_AESGCMCreate(const void *key)
{
	MTY_AESGCM *ctx = MTY_Alloc(1, sizeof(MTY_AESGCM));
	bool r = true;

	NTSTATUS e = BCryptOpenAlgorithmProvider(&ctx->ahandle, BCRYPT_AES_ALGORITHM, NULL, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptOpenAlgorithmProvider' failed with error 0x%X", e);
		r = false;
		goto except;
	}

	e = BCryptSetProperty(ctx->ahandle, BCRYPT_CHAINING_MODE, (UCHAR *) BCRYPT_CHAIN_MODE_GCM,
		(ULONG) sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptSetProperty' failed with error 0x%X", e);
		r = false;
		goto except;
	}

	e = BCryptGenerateSymmetricKey(ctx->ahandle, &ctx->khandle, NULL, 0, (UCHAR *) key, 16, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptGenerateSymmetricKey' failed with error 0x%X", e);
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

	if (ctx->khandle)
		BCryptDestroyKey(ctx->khandle);

	if (ctx->ahandle)
		BCryptCloseAlgorithmProvider(ctx->ahandle, 0);

	MTY_Free(ctx);
	*aesgcm = NULL;
}

bool MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *nonce, const void *plainText, size_t size,
	void *tag, void *cipherText)
{
	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info = {0};
	BCRYPT_INIT_AUTH_MODE_INFO(info);
	info.pbNonce = (UCHAR *) nonce;
	info.cbNonce = 12;
	info.pbTag = tag;
	info.cbTag = 16;

	ULONG output = 0;
	NTSTATUS e = BCryptEncrypt(ctx->khandle, (UCHAR *) plainText, (ULONG) size, &info,
		NULL, 0, cipherText, (ULONG) size, &output, 0);

	return e == STATUS_SUCCESS;
}

bool MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *nonce, const void *cipherText, size_t size,
	const void *tag, void *plainText)
{
	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info = {0};
	BCRYPT_INIT_AUTH_MODE_INFO(info);
	info.pbNonce = (UCHAR *) nonce;
	info.cbNonce = 12;
	info.pbTag = (UCHAR *) tag;
	info.cbTag = 16;

	ULONG output = 0;
	NTSTATUS e = BCryptDecrypt(ctx->khandle, (UCHAR *) cipherText, (ULONG) size, &info,
		NULL, 0, plainText, (ULONG) size, &output, 0);

	return e == STATUS_SUCCESS;
}
