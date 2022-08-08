// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdbool.h>

#if !defined(LIBCRYPTO_EXTERN)
	#define LIBCRYPTO_EXTERN extern
#endif

#define EVP_CTRL_AEAD_GET_TAG 0x10
#define EVP_CTRL_GCM_GET_TAG  EVP_CTRL_AEAD_GET_TAG

#define EVP_CTRL_AEAD_SET_TAG 0x11
#define EVP_CTRL_GCM_SET_TAG  EVP_CTRL_AEAD_SET_TAG

typedef struct engine_st ENGINE;
typedef struct evp_cipher_st EVP_CIPHER;
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;
typedef struct evp_md_st EVP_MD;

LIBCRYPTO_EXTERN const EVP_CIPHER *(*EVP_aes_128_gcm)(void);
LIBCRYPTO_EXTERN EVP_CIPHER_CTX *(*EVP_CIPHER_CTX_new)(void);
LIBCRYPTO_EXTERN void (*EVP_CIPHER_CTX_free)(EVP_CIPHER_CTX *c);
LIBCRYPTO_EXTERN int (*EVP_CipherInit_ex)(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
	const unsigned char *key, const unsigned char *iv, int enc);
LIBCRYPTO_EXTERN int (*EVP_EncryptUpdate)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
	const unsigned char *in, int inl);
LIBCRYPTO_EXTERN int (*EVP_DecryptUpdate)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
	const unsigned char *in, int inl);
LIBCRYPTO_EXTERN int (*EVP_EncryptFinal_ex)(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl);
LIBCRYPTO_EXTERN int (*EVP_DecryptFinal_ex)(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl);
LIBCRYPTO_EXTERN int (*EVP_CIPHER_CTX_ctrl)(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr);

LIBCRYPTO_EXTERN const EVP_MD *(*EVP_sha1)(void);
LIBCRYPTO_EXTERN const EVP_MD *(*EVP_sha256)(void);
LIBCRYPTO_EXTERN unsigned char *(*SHA1)(const unsigned char *d, size_t n, unsigned char *md);
LIBCRYPTO_EXTERN unsigned char *(*SHA256)(const unsigned char *d, size_t n, unsigned char *md);
LIBCRYPTO_EXTERN unsigned char *(*HMAC)(const EVP_MD *evp_md, const void *key, int key_len,
	const unsigned char *d, size_t n, unsigned char *md, unsigned int *md_len);

LIBCRYPTO_EXTERN int (*RAND_bytes)(unsigned char *buf, int num);

bool mty_libcrypto_global_init(void);
