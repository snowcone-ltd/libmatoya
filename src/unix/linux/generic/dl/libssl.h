// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "sym.h"


// Interface

#define SSL_ERROR_WANT_READ                           2
#define SSL_ERROR_WANT_WRITE                          3
#define SSL_ERROR_ZERO_RETURN                         6

#define SSL_CTRL_SET_MTU                              17
#define SSL_CTRL_OPTIONS                              32
#define SSL_CTRL_SET_TLSEXT_HOSTNAME                  55
#define SSL_CTRL_SET_TLSEXT_STATUS_REQ_TYPE           65
#define SSL_CTRL_SET_VERIFY_CERT_STORE                106

#define TLSEXT_NAMETYPE_host_name                     0
#define TLSEXT_STATUSTYPE_ocsp                        1

#define SSL_VERIFY_PEER                               0x01
#define SSL_VERIFY_FAIL_IF_NO_PEER_CERT               0x02
#define SSL_OP_NO_SSLv2                               0x0
#define SSL_OP_NO_QUERY_MTU                           0x00001000U
#define SSL_OP_NO_TICKET                              0x00004000U
#define SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION 0x00010000U
#define SSL_OP_NO_COMPRESSION                         0x00020000U
#define SSL_OP_NO_SSLv3                               0x02000000U

#define NID_rsaEncryption                             6
#define EVP_PKEY_RSA                                  NID_rsaEncryption

#define MBSTRING_FLAG                                 0x1000
#define MBSTRING_ASC                                  (MBSTRING_FLAG | 1)
#define X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS          0x4
#define RSA_F4                                        0x10001L
#define EVP_MAX_MD_SIZE                               64

typedef struct rsa_st RSA;
typedef struct bio_st BIO;
typedef struct bignum_st BIGNUM;
typedef struct x509_st X509;
typedef struct X509_name_st X509_NAME;
typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct X509_VERIFY_PARAM_st X509_VERIFY_PARAM;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct bn_gencb_st BN_GENCB;
typedef struct asn1_string_st ASN1_INTEGER;
typedef struct asn1_string_st ASN1_TIME;
typedef struct evp_md_st EVP_MD;
typedef struct bio_method_st BIO_METHOD;

typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
typedef struct ssl_st SSL;

typedef int (*SSL_verify_cb)(int preverify_ok, X509_STORE_CTX *x509_ctx);
typedef int pem_password_cb(char *buf, int size, int rwflag, void *userdata);

static int (*SSL_library_init)(void);

static SSL *(*SSL_new)(SSL_CTX *ctx);
static void (*SSL_free)(SSL *ssl);
static int (*SSL_read)(SSL *ssl, void *buf, int num);
static int (*SSL_write)(SSL *ssl, const void *buf, int num);
static void (*SSL_set_verify)(SSL *s, int mode, SSL_verify_cb callback);
static void (*SSL_set_verify_depth)(SSL *s, int depth);
static int (*SSL_get_error)(const SSL *s, int ret_code);
static X509_VERIFY_PARAM *(*SSL_get0_param)(SSL *ssl);
static long (*SSL_ctrl)(SSL *ssl, int cmd, long larg, void *parg);
static void (*SSL_set_bio)(SSL *s, BIO *rbio, BIO *wbio);
static void (*SSL_set_connect_state)(SSL *s);
static int (*SSL_do_handshake)(SSL *s);
static int (*SSL_use_certificate)(SSL *ssl, X509 *x);
static X509 *(*SSL_get_peer_certificate)(const SSL *s);
static int (*SSL_use_RSAPrivateKey)(SSL *ssl, RSA *rsa);

static const SSL_METHOD *(*TLSv1_2_method)(void);
static const SSL_METHOD *(*DTLS_method)(void);
static SSL_CTX *(*SSL_CTX_new)(const SSL_METHOD *meth);
static int (*SSL_CTX_set_default_verify_paths)(SSL_CTX *ctx);

static void (*SSL_CTX_free)(SSL_CTX *);

static BIO *(*BIO_new)(const BIO_METHOD *type);
static int (*BIO_free)(BIO *a);
static const BIO_METHOD *(*BIO_s_mem)(void);
static int (*BIO_write)(BIO *b, const void *data, int len);
static size_t (*BIO_ctrl_pending)(BIO *b);
static int (*BIO_read)(BIO *b, void *data, int len);

static int (*X509_VERIFY_PARAM_set1_host)(X509_VERIFY_PARAM *param, const char *name, size_t namelen);
static void (*X509_VERIFY_PARAM_set_hostflags)(X509_VERIFY_PARAM *param, unsigned int flags);
static void (*X509_free)(X509 *a);
static X509 *(*X509_new)(void);
static int (*X509_set_pubkey)(X509 *x, EVP_PKEY *pkey);
static int (*X509_sign)(X509 *x, EVP_PKEY *pkey, const EVP_MD *md);
static int (*X509_digest)(const X509 *data, const EVP_MD *type, unsigned char *md, unsigned int *len);
// static ASN1_TIME *(*X509_getm_notBefore)(const X509 *x);
// static ASN1_TIME *(*X509_getm_notAfter)(const X509 *x);
static int (*X509_set_version)(X509 *x, long version);
static int (*X509_set_issuer_name)(X509 *x, X509_NAME *name);
static X509_NAME *(*X509_get_subject_name)(const X509 *a);
static ASN1_INTEGER *(*X509_get_serialNumber)(X509 *x);
static ASN1_TIME *(*X509_gmtime_adj)(ASN1_TIME *s, long adj);
static int (*X509_NAME_add_entry_by_txt)(X509_NAME *name, const char *field, int type,
	const unsigned char *bytes, int len, int loc, int set);

static BIGNUM *(*BN_new)(void);
static void (*BN_free)(BIGNUM *a);
static int (*BN_set_word)(BIGNUM *a, unsigned long long w);

static EVP_PKEY *(*EVP_PKEY_new)(void);
static const EVP_MD *(*EVP_sha256)(void);
static int (*EVP_PKEY_assign)(EVP_PKEY *pkey, int type, void *key);

static RSA *(*RSA_new)(void);
static void (*RSA_free)(RSA *r);
static int (*RSA_generate_key_ex)(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb);

static int (*ASN1_INTEGER_set)(ASN1_INTEGER *a, long v);


// Runtime open

static MTY_Atomic32 LIBSSL_LOCK;
static MTY_SO *LIBSSL_SO;
static bool LIBSSL_INIT;

static void __attribute__((destructor)) libssl_global_destroy(void)
{
	MTY_GlobalLock(&LIBSSL_LOCK);

	MTY_SOUnload(&LIBSSL_SO);
	LIBSSL_INIT = false;

	MTY_GlobalUnlock(&LIBSSL_LOCK);
}

static bool libssl_global_init(void)
{
	MTY_GlobalLock(&LIBSSL_LOCK);

	if (!LIBSSL_INIT) {
		bool r = true;
		bool library_init = false;
		LIBSSL_SO = MTY_SOLoad("libssl.so.1.1");

		if (!LIBSSL_SO) {
			LIBSSL_SO = MTY_SOLoad("libssl.so.1.0.0");
			library_init = true;
		}

		if (!LIBSSL_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBSSL_SO, SSL_new);
		LOAD_SYM(LIBSSL_SO, SSL_free);
		LOAD_SYM(LIBSSL_SO, SSL_read);
		LOAD_SYM(LIBSSL_SO, SSL_write);
		LOAD_SYM(LIBSSL_SO, SSL_set_verify);
		LOAD_SYM(LIBSSL_SO, SSL_set_verify_depth);
		LOAD_SYM(LIBSSL_SO, SSL_get_error);
		LOAD_SYM(LIBSSL_SO, SSL_get0_param);
		LOAD_SYM(LIBSSL_SO, SSL_ctrl);
		LOAD_SYM(LIBSSL_SO, SSL_set_bio);
		LOAD_SYM(LIBSSL_SO, SSL_set_connect_state);
		LOAD_SYM(LIBSSL_SO, SSL_do_handshake);
		LOAD_SYM(LIBSSL_SO, SSL_use_certificate);
		LOAD_SYM(LIBSSL_SO, SSL_get_peer_certificate);
		LOAD_SYM(LIBSSL_SO, SSL_use_RSAPrivateKey);

		LOAD_SYM(LIBSSL_SO, TLSv1_2_method);
		LOAD_SYM(LIBSSL_SO, DTLS_method);
		LOAD_SYM(LIBSSL_SO, SSL_CTX_new);
		LOAD_SYM(LIBSSL_SO, SSL_CTX_free);
		LOAD_SYM(LIBSSL_SO, SSL_CTX_set_default_verify_paths);

		LOAD_SYM(LIBSSL_SO, BIO_new);
		LOAD_SYM(LIBSSL_SO, BIO_s_mem);
		LOAD_SYM(LIBSSL_SO, BIO_write);
		LOAD_SYM(LIBSSL_SO, BIO_ctrl_pending);
		LOAD_SYM(LIBSSL_SO, BIO_read);
		LOAD_SYM(LIBSSL_SO, BIO_free);

		LOAD_SYM(LIBSSL_SO, X509_VERIFY_PARAM_set1_host);
		LOAD_SYM(LIBSSL_SO, X509_VERIFY_PARAM_set_hostflags);
		LOAD_SYM(LIBSSL_SO, X509_new);
		LOAD_SYM(LIBSSL_SO, X509_free);
		LOAD_SYM(LIBSSL_SO, X509_set_pubkey);
		LOAD_SYM(LIBSSL_SO, X509_sign);
		LOAD_SYM(LIBSSL_SO, X509_digest);
		// LOAD_SYM(LIBSSL_SO, X509_getm_notBefore);
		// LOAD_SYM(LIBSSL_SO, X509_getm_notAfter);
		LOAD_SYM(LIBSSL_SO, X509_set_version);
		LOAD_SYM(LIBSSL_SO, X509_set_issuer_name);
		LOAD_SYM(LIBSSL_SO, X509_get_subject_name);
		LOAD_SYM(LIBSSL_SO, X509_get_serialNumber);
		LOAD_SYM(LIBSSL_SO, X509_gmtime_adj);
		LOAD_SYM(LIBSSL_SO, X509_NAME_add_entry_by_txt);

		LOAD_SYM(LIBSSL_SO, RSA_new);
		LOAD_SYM(LIBSSL_SO, RSA_free);
		LOAD_SYM(LIBSSL_SO, RSA_generate_key_ex);

		LOAD_SYM(LIBSSL_SO, BN_new);
		LOAD_SYM(LIBSSL_SO, BN_free);
		LOAD_SYM(LIBSSL_SO, BN_set_word);

		LOAD_SYM(LIBSSL_SO, EVP_PKEY_new);
		LOAD_SYM(LIBSSL_SO, EVP_sha256);
		LOAD_SYM(LIBSSL_SO, EVP_PKEY_assign);

		LOAD_SYM(LIBSSL_SO, ASN1_INTEGER_set);

		if (library_init) {
			LOAD_SYM(LIBSSL_SO, SSL_library_init);
			SSL_library_init();
		}

		except:

		if (!r)
			libssl_global_destroy();

		LIBSSL_INIT = r;
	}

	MTY_GlobalUnlock(&LIBSSL_LOCK);

	return LIBSSL_INIT;
}
