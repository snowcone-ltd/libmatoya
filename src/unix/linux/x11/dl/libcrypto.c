// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"
#include "libcrypto.h"

#include "sym.h"

static const EVP_CIPHER *(*EVP_aes_128_gcm)(void);
static EVP_CIPHER_CTX *(*EVP_CIPHER_CTX_new)(void);
static void (*EVP_CIPHER_CTX_free)(EVP_CIPHER_CTX *c);
static int (*EVP_CipherInit_ex)(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
	const unsigned char *key, const unsigned char *iv, int enc);
static int (*EVP_EncryptUpdate)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
	const unsigned char *in, int inl);
static int (*EVP_DecryptUpdate)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
	const unsigned char *in, int inl);
static int (*EVP_EncryptFinal_ex)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);
static int (*EVP_DecryptFinal_ex)(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl);
static int (*EVP_CIPHER_CTX_ctrl)(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr);
static const EVP_MD *(*EVP_sha1)(void);
static const EVP_MD *(*EVP_sha256)(void);
static unsigned char *(*SHA1)(const unsigned char *d, size_t n, unsigned char *md);
static unsigned char *(*SHA256)(const unsigned char *d, size_t n, unsigned char *md);
static unsigned char *(*HMAC)(const EVP_MD *evp_md, const void *key, int key_len,
	const unsigned char *d, size_t n, unsigned char *md, unsigned int *md_len);
static int (*RAND_bytes)(unsigned char *buf, int num);

static MTY_Atomic32 LIBCRYPTO_LOCK;
static MTY_SO *LIBCRYPTO_SO;
static bool LIBCRYPTO_INIT;

static void __attribute__((destructor)) libcrypto_global_destroy(void)
{
	MTY_GlobalLock(&LIBCRYPTO_LOCK);

	MTY_SOUnload(&LIBCRYPTO_SO);
	LIBCRYPTO_INIT = false;

	MTY_GlobalUnlock(&LIBCRYPTO_LOCK);
}

static bool libcrypto_global_init(void)
{
	MTY_GlobalLock(&LIBCRYPTO_LOCK);

	if (!LIBCRYPTO_INIT) {
		bool r = true;
		LIBCRYPTO_SO = MTY_SOLoad("libcrypto.so.3");

		if (!LIBCRYPTO_SO)
			LIBCRYPTO_SO = MTY_SOLoad("libcrypto.so.1.1");

		if (!LIBCRYPTO_SO)
			LIBCRYPTO_SO = MTY_SOLoad("libcrypto.so.1.0.0");

		if (!LIBCRYPTO_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBCRYPTO_SO, EVP_aes_128_gcm);
		LOAD_SYM(LIBCRYPTO_SO, EVP_CIPHER_CTX_new);
		LOAD_SYM(LIBCRYPTO_SO, EVP_CIPHER_CTX_free);
		LOAD_SYM(LIBCRYPTO_SO, EVP_CipherInit_ex);
		LOAD_SYM(LIBCRYPTO_SO, EVP_EncryptUpdate);
		LOAD_SYM(LIBCRYPTO_SO, EVP_DecryptUpdate);
		LOAD_SYM(LIBCRYPTO_SO, EVP_EncryptFinal_ex);
		LOAD_SYM(LIBCRYPTO_SO, EVP_DecryptFinal_ex);
		LOAD_SYM(LIBCRYPTO_SO, EVP_CIPHER_CTX_ctrl);
		LOAD_SYM(LIBCRYPTO_SO, EVP_sha1);
		LOAD_SYM(LIBCRYPTO_SO, EVP_sha256);
		LOAD_SYM(LIBCRYPTO_SO, SHA1);
		LOAD_SYM(LIBCRYPTO_SO, SHA256);
		LOAD_SYM(LIBCRYPTO_SO, HMAC);
		LOAD_SYM(LIBCRYPTO_SO, RAND_bytes);

		except:

		if (!r)
			libcrypto_global_destroy();

		LIBCRYPTO_INIT = r;
	}

	MTY_GlobalUnlock(&LIBCRYPTO_LOCK);

	return LIBCRYPTO_INIT;
}
