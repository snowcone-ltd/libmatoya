// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(__aarch64__)
	#include "aes-gcm-neon.h"

#else
	#include <immintrin.h>
#endif


// SSSE3, SSE2

#define SWAP64(i) \
	_mm_shuffle_epi8(i, _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15))

#define CBINCR(i) \
	SWAP64(_mm_add_epi32(SWAP64(i), _mm_set_epi32(0, 0, 0, 1)))


// AESNI, SSE2

static void aes_key_expansion(const uint8_t *key, __m128i *k)
{
	k[0] = _mm_loadu_si128((const __m128i *) key);

	for (uint8_t x = 0; x < 10; x++) {
		__m128i keygen = _mm_setzero_si128();

		switch (x) {
			case 0: keygen = _mm_aeskeygenassist_si128(k[x], 0x01); break;
			case 1: keygen = _mm_aeskeygenassist_si128(k[x], 0x02); break;
			case 2: keygen = _mm_aeskeygenassist_si128(k[x], 0x04); break;
			case 3: keygen = _mm_aeskeygenassist_si128(k[x], 0x08); break;
			case 4: keygen = _mm_aeskeygenassist_si128(k[x], 0x10); break;
			case 5: keygen = _mm_aeskeygenassist_si128(k[x], 0x20); break;
			case 6: keygen = _mm_aeskeygenassist_si128(k[x], 0x40); break;
			case 7: keygen = _mm_aeskeygenassist_si128(k[x], 0x80); break;
			case 8: keygen = _mm_aeskeygenassist_si128(k[x], 0x1B); break;
			case 9: keygen = _mm_aeskeygenassist_si128(k[x], 0x36); break;
		}

		keygen = _mm_shuffle_epi32(keygen, _MM_SHUFFLE(3, 3, 3, 3));

		__m128i tmp = _mm_xor_si128(k[x], _mm_slli_si128(k[x], 4));
		tmp = _mm_xor_si128(tmp, _mm_slli_si128(tmp, 4));
		tmp = _mm_xor_si128(tmp, _mm_slli_si128(tmp, 4));

		k[x + 1] = _mm_xor_si128(tmp, keygen);
	}
}

static __m128i aes(const __m128i *k, __m128i plain)
{
	__m128i t = _mm_xor_si128(plain, k[0]);

	t = _mm_aesenc_si128(t, k[1]);
	t = _mm_aesenc_si128(t, k[2]);
	t = _mm_aesenc_si128(t, k[3]);
	t = _mm_aesenc_si128(t, k[4]);
	t = _mm_aesenc_si128(t, k[5]);
	t = _mm_aesenc_si128(t, k[6]);
	t = _mm_aesenc_si128(t, k[7]);
	t = _mm_aesenc_si128(t, k[8]);
	t = _mm_aesenc_si128(t, k[9]);

	return _mm_aesenclast_si128(t, k[10]);
}


// PCLMULQDQ, SSE2

static __m128i gcm_gfmul(__m128i a, __m128i b)
{
	__m128i t1 = _mm_clmulepi64_si128(a, b, 0x00);
	__m128i t2 = _mm_clmulepi64_si128(a, b, 0x10);
	__m128i t3 = _mm_clmulepi64_si128(a, b, 0x01);
	__m128i t4 = _mm_clmulepi64_si128(a, b, 0x11);
	t2 = _mm_xor_si128(t2, t3);
	t3 = _mm_slli_si128(t2, 8);
	t2 = _mm_srli_si128(t2, 8);
	t1 = _mm_xor_si128(t1, t3);
	t4 = _mm_xor_si128(t4, t2);

	__m128i t5 = _mm_srli_epi32(t1, 31);
	__m128i t6 = _mm_srli_epi32(t4, 31);
	t1 = _mm_slli_epi32(t1, 1);
	t4 = _mm_slli_epi32(t4, 1);

	__m128i t7 = _mm_srli_si128(t5, 12);
	t6 = _mm_slli_si128(t6, 4);
	t5 = _mm_slli_si128(t5, 4);
	t1 = _mm_or_si128(t1, t5);
	t4 = _mm_or_si128(t4, t6);
	t4 = _mm_or_si128(t4, t7);
	t5 = _mm_slli_epi32(t1, 31);
	t6 = _mm_slli_epi32(t1, 30);
	t7 = _mm_slli_epi32(t1, 25);
	t5 = _mm_xor_si128(t5, t6);
	t5 = _mm_xor_si128(t5, t7);
	t6 = _mm_srli_si128(t5, 4);
	t5 = _mm_slli_si128(t5, 12);
	t1 = _mm_xor_si128(t1, t5);

	__m128i t0 = _mm_srli_epi32(t1, 1);
	t2 = _mm_srli_epi32(t1, 2);
	t3 = _mm_srli_epi32(t1, 7);
	t0 = _mm_xor_si128(t0, t2);
	t0 = _mm_xor_si128(t0, t3);
	t0 = _mm_xor_si128(t0, t6);
	t1 = _mm_xor_si128(t1, t0);

	return _mm_xor_si128(t4, t1);
}

static __m128i gcm_gfmul4(__m128i H1, __m128i H2, __m128i H3, __m128i H4,
	__m128i X1, __m128i X2, __m128i X3, __m128i X4)
{
	// Algorithm by Krzysztof Jankowski, Pierre Laurent - Intel

	__m128i H1_X1_lo = _mm_clmulepi64_si128(H1, X1, 0x00);
	__m128i H2_X2_lo = _mm_clmulepi64_si128(H2, X2, 0x00);
	__m128i H3_X3_lo = _mm_clmulepi64_si128(H3, X3, 0x00);
	__m128i H4_X4_lo = _mm_clmulepi64_si128(H4, X4, 0x00);

	__m128i lo = _mm_xor_si128(H1_X1_lo, H2_X2_lo);
	lo = _mm_xor_si128(lo, H3_X3_lo);
	lo = _mm_xor_si128(lo, H4_X4_lo);

	__m128i H1_X1_hi = _mm_clmulepi64_si128(H1, X1, 0x11);
	__m128i H2_X2_hi = _mm_clmulepi64_si128(H2, X2, 0x11);
	__m128i H3_X3_hi = _mm_clmulepi64_si128(H3, X3, 0x11);
	__m128i H4_X4_hi = _mm_clmulepi64_si128(H4, X4, 0x11);

	__m128i hi = _mm_xor_si128(H1_X1_hi, H2_X2_hi);
	hi = _mm_xor_si128(hi, H3_X3_hi);
	hi = _mm_xor_si128(hi, H4_X4_hi);

	__m128i t0 = _mm_shuffle_epi32(H1, 78);
	__m128i t4 = _mm_shuffle_epi32(X1, 78);
	t0 = _mm_xor_si128(t0, H1);
	t4 = _mm_xor_si128(t4, X1);

	__m128i t1 = _mm_shuffle_epi32(H2, 78);
	__m128i t5 = _mm_shuffle_epi32(X2, 78);
	t1 = _mm_xor_si128(t1, H2);
	t5 = _mm_xor_si128(t5, X2);

	__m128i t2 = _mm_shuffle_epi32(H3, 78);
	__m128i t6 = _mm_shuffle_epi32(X3, 78);
	t2 = _mm_xor_si128(t2, H3);
	t6 = _mm_xor_si128(t6, X3);

	__m128i t3 = _mm_shuffle_epi32(H4, 78);
	__m128i t7 = _mm_shuffle_epi32(X4, 78);
	t3 = _mm_xor_si128(t3, H4);
	t7 = _mm_xor_si128(t7, X4);

	t0 = _mm_clmulepi64_si128(t0, t4, 0x00);
	t1 = _mm_clmulepi64_si128(t1, t5, 0x00);
	t2 = _mm_clmulepi64_si128(t2, t6, 0x00);
	t3 = _mm_clmulepi64_si128(t3, t7, 0x00);

	t0 = _mm_xor_si128(t0, lo);
	t0 = _mm_xor_si128(t0, hi);
	t0 = _mm_xor_si128(t1, t0);
	t0 = _mm_xor_si128(t2, t0);
	t0 = _mm_xor_si128(t3, t0);

	t4 = _mm_slli_si128(t0, 8);
	t0 = _mm_srli_si128(t0, 8);

	t3 = _mm_xor_si128(t4, lo);
	t6 = _mm_xor_si128(t0, hi);

	__m128i t8 = _mm_srli_epi32(t6, 31);
	t7 = _mm_srli_epi32(t3, 31);
	t3 = _mm_slli_epi32(t3, 1);
	t6 = _mm_slli_epi32(t6, 1);

	__m128i t9 = _mm_srli_si128(t7, 12);
	t8 = _mm_slli_si128(t8, 4);
	t7 = _mm_slli_si128(t7, 4);
	t3 = _mm_or_si128(t3, t7);
	t6 = _mm_or_si128(t6, t8);
	t6 = _mm_or_si128(t6, t9);

	t7 = _mm_slli_epi32(t3, 31);
	t8 = _mm_slli_epi32(t3, 30);
	t9 = _mm_slli_epi32(t3, 25);

	t7 = _mm_xor_si128(t7, t8);
	t7 = _mm_xor_si128(t7, t9);
	t8 = _mm_srli_si128(t7, 4);
	t7 = _mm_slli_si128(t7, 12);
	t3 = _mm_xor_si128(t3, t7);

	t2 = _mm_srli_epi32(t3, 1);
	t4 = _mm_srli_epi32(t3, 2);
	t5 = _mm_srli_epi32(t3, 7);
	t2 = _mm_xor_si128(t2, t4);
	t2 = _mm_xor_si128(t2, t5);
	t2 = _mm_xor_si128(t2, t8);
	t3 = _mm_xor_si128(t3, t2);

	return _mm_xor_si128(t6, t3);
}


// AES-GCM (SSE2)

typedef union {
	uint8_t u8[16];
	uint32_t u32[4];
	uint64_t u64[2];
} P128;

static __m128i gcm_ghash16(__m128i H, __m128i in, __m128i ghash)
{
	ghash = _mm_xor_si128(ghash, SWAP64(in));

	return gcm_gfmul(ghash, H);
}

static __m128i aes_gcm(const __m128i *k, const __m128i *H, __m128i iv,
	const P128 *x, P128 *y, const P128 *crypt, size_t size)
{
	__m128i ghash = _mm_setzero_si128();
	__m128i cb = CBINCR(iv);

	size_t n4 = size / 64;
	size_t n = size / 16;
	size_t rem = size  % 16;

	// 4 blocks at a time
	for (size_t i = 0; i < n4; i++) {
		// XXX The code within this loop must be aggressively optimized

		__m128i yi[4];
		yi[0] = aes(k, cb);
		cb = CBINCR(cb);

		yi[1] = aes(k, cb);
		cb = CBINCR(cb);

		yi[2] = aes(k, cb);
		cb = CBINCR(cb);

		yi[3] = aes(k, cb);
		cb = CBINCR(cb);

		__m128i xi[4];
		xi[0] = _mm_loadu_si128((const __m128i *) &x[i * 4]);
		xi[1] = _mm_loadu_si128((const __m128i *) &x[i * 4 + 1]);
		xi[2] = _mm_loadu_si128((const __m128i *) &x[i * 4 + 2]);
		xi[3] = _mm_loadu_si128((const __m128i *) &x[i * 4 + 3]);

		yi[0] = _mm_xor_si128(yi[0], xi[0]);
		yi[1] = _mm_xor_si128(yi[1], xi[1]);
		yi[2] = _mm_xor_si128(yi[2], xi[2]);
		yi[3] = _mm_xor_si128(yi[3], xi[3]);

		_mm_storeu_si128((__m128i *) &y[i * 4], yi[0]);
		_mm_storeu_si128((__m128i *) &y[i * 4 + 1], yi[1]);
		_mm_storeu_si128((__m128i *) &y[i * 4 + 2], yi[2]);
		_mm_storeu_si128((__m128i *) &y[i * 4 + 3], yi[3]);

		__m128i in[4];
		in[0] = SWAP64(crypt == x ? xi[0] : yi[0]);
		in[1] = SWAP64(crypt == x ? xi[1] : yi[1]);
		in[2] = SWAP64(crypt == x ? xi[2] : yi[2]);
		in[3] = SWAP64(crypt == x ? xi[3] : yi[3]);

		in[0] = _mm_xor_si128(ghash, in[0]);
		ghash = gcm_gfmul4(H[0], H[1], H[2], H[3], in[3], in[2], in[1], in[0]);
	}

	// 1 block at a time
	for (size_t i = n4 * 4; i < n; i++) {
		__m128i yi = aes(k, cb);
		cb = CBINCR(cb);

		__m128i xi = _mm_loadu_si128((const __m128i *) &x[i]);
		yi = _mm_xor_si128(yi, xi);
		_mm_storeu_si128((__m128i *) &y[i], yi);
		ghash = gcm_ghash16(H[0], crypt == x ? xi : yi, ghash);
	}

	// Remaining data in last block
	if (rem > 0) {
		__m128i tmp2 = _mm_setzero_si128();
		__m128i tmp = aes(k, cb);

		for (size_t i = 0; i < rem; i++) {
			y[n].u8[i] = x[n].u8[i] ^ ((uint8_t *) &tmp)[i];
			((uint8_t *) &tmp2)[i] = crypt[n].u8[i];
		}

		ghash = gcm_ghash16(H[0], tmp2, ghash);
	}

	return ghash;
}

static void aes_gcm_full(const __m128i *k, const __m128i *H, const P128 *nonce, const P128 *crypt,
	const P128 *in, P128 *out, size_t size, P128 *tag)
{
	// Set up the IV with 12 bytes from the nonce and the 4 byte counter
	__m128i iv = _mm_set_epi32(0x01000000, nonce->u32[2], nonce->u32[1], nonce->u32[0]);

	// Encrypt or decrypt the data while generating the ghash
	__m128i ghash = aes_gcm(k, H, iv, in, out, crypt, size);

	// ghash needs to be multiplied by the length of the data then reversed
	__m128i len = SWAP64(_mm_set_epi64x(0, size * 8));
	ghash = gcm_ghash16(H[0], len, ghash);
	ghash = SWAP64(ghash);

	// Encrypt the tag
	__m128i tmp = aes(k, iv);
	ghash = _mm_xor_si128(tmp, ghash);
	_mm_storeu_si128((__m128i *) tag, ghash);
}


// Public

struct MTY_AESGCM {
	__m128i k[11];
	__m128i H[4];
};

MTY_AESGCM *MTY_AESGCMCreate(const void *key, size_t keySize)
{
	if (keySize != 16)
		return NULL;

	MTY_AESGCM *ctx = MTY_AllocAligned(sizeof(MTY_AESGCM), keySize);

	aes_key_expansion(key, ctx->k);

	__m128i H = aes(ctx->k, _mm_setzero_si128());

	ctx->H[0] = SWAP64(H);
	ctx->H[1] = gcm_gfmul(ctx->H[0], ctx->H[0]);
	ctx->H[2] = gcm_gfmul(ctx->H[0], ctx->H[1]);
	ctx->H[3] = gcm_gfmul(ctx->H[0], ctx->H[2]);

	return ctx;
}

void MTY_AESGCMDestroy(MTY_AESGCM **aesgcm)
{
	if (!aesgcm || !*aesgcm)
		return;

	MTY_AESGCM *ctx = *aesgcm;

	memset(ctx, 0, sizeof(MTY_AESGCM));

	MTY_FreeAligned(ctx);
	*aesgcm = NULL;
}

bool MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *nonce, const void *plainText, size_t size,
	void *tag, void *cipherText)
{
	aes_gcm_full(ctx->k, ctx->H, nonce, cipherText, plainText, cipherText, size, tag);

	return true;
}

bool MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *nonce, const void *cipherText, size_t size,
	const void *tag, void *plainText)
{
	P128 tag128;
	aes_gcm_full(ctx->k, ctx->H, nonce, cipherText, cipherText, plainText, size, &tag128);

	const P128 *itag = tag;

	if (tag128.u64[0] != itag->u64[0] || tag128.u64[1] != itag->u64[1])
		return false;

	return true;
}
