// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

// https://github.com/DLTcollab/sse2neon

#pragma once

#include <arm_neon.h>

typedef int64x2_t __m128i;

#define _MM_SHUFFLE(fp3, fp2, fp1, fp0) \
	(((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

#define _mm_loadu_si128(x) \
	vreinterpretq_s64_s32(vld1q_s32((const int32_t *) (x)))

#define _mm_setzero_si128() \
	vreinterpretq_s64_s32(vdupq_n_s32(0))

#define _mm_xor_si128(a, b) \
	vreinterpretq_s64_s32(veorq_s32(vreinterpretq_s32_s64(a), vreinterpretq_s32_s64(b)))

#define _mm_or_si128(a, b) \
	vreinterpretq_s64_s32(vorrq_s32(vreinterpretq_s32_s64(a), vreinterpretq_s32_s64(b)))

#define _mm_storeu_si128(p, a) \
	vst1q_s32((int32_t *) (p), vreinterpretq_s32_s64(a))

#define _mm_set_epi64x(i1, i2) \
	vcombine_s64(vcreate_s64(i2), vcreate_s64(i1))

#define _mm_add_epi32(a, b) \
	vreinterpretq_s64_s32(vaddq_s32(vreinterpretq_s32_s64(a), vreinterpretq_s32_s64(b)))

#define _mm_slli_epi32(a, imm) \
	((imm) <= 0) ? (a) : (imm) > 31 ? _mm_setzero_si128() : \
		vreinterpretq_s64_s32(vshlq_s32(vreinterpretq_s32_s64(a), vdupq_n_s32(imm)))

#define _mm_shuffle_epi32(a, imm) \
	__extension__({ \
		int32x4_t _input = vreinterpretq_s32_s64(a); \
		int32x4_t _shuf = __builtin_shufflevector( \
			_input, _input, (imm) & (0x3), ((imm) >> 2) & 0x3, \
			((imm) >> 4) & 0x3, ((imm) >> 6) & 0x3); \
		vreinterpretq_s64_s32(_shuf); \
	})

#define _mm_slli_si128(a, imm) \
	__extension__({ \
		__m128i ret; \
		if ((imm) <= 0) \
			ret = a; \
		if ((imm) > 15) { \
			ret = _mm_setzero_si128(); \
		} else { \
			ret = vreinterpretq_s64_s8(vextq_s8( \
				vdupq_n_s8(0), vreinterpretq_s8_s64(a), 16 - (imm))); \
		} \
		ret; \
	})

#define _mm_srli_si128(a, imm) \
	__extension__({ \
		__m128i ret; \
		if ((imm) <= 0) \
			ret = a; \
		if ((imm) > 15) { \
			ret = _mm_setzero_si128(); \
		} else { \
			ret = vreinterpretq_s64_s8( \
				vextq_s8(vreinterpretq_s8_s64(a), vdupq_n_s8(0), (imm))); \
		} \
		ret; \
	})

#define _mm_srli_epi32(a, imm) \
	__extension__({ \
		__m128i ret; \
		if ((imm) == 0) \
			ret = a; \
		if (0 < (imm) && (imm) < 32) { \
			ret = vreinterpretq_s64_u32( \
				vshlq_u32(vreinterpretq_u32_s64(a), vdupq_n_s32(-imm))); \
		} else { \
			ret = _mm_setzero_si128(); \
		} \
		ret; \
	})

#define _mm_shuffle_epi8(a, b) \
	vreinterpretq_s64_s8(vqtbl1q_s8(vreinterpretq_s8_s64(a), \
		vandq_u8(vreinterpretq_u8_s64(b), vdupq_n_u8(0x8F))))

#define _mm_aesenc_si128(a, b) \
	vreinterpretq_s64_u8(vaesmcq_u8(vaeseq_u8(vreinterpretq_u8_s64(a), vdupq_n_u8(0))) \
		^ vreinterpretq_u8_s64(b))

#define _mm_aesenclast_si128(a, RoundKey) \
	_mm_xor_si128(vreinterpretq_s64_u8(vaeseq_u8(vreinterpretq_u8_s64(a), \
		vdupq_n_u8(0))), RoundKey)

#define _vmull_p64(a, b) \
	vreinterpretq_u64_p128(vmull_p64(vget_lane_p64(vreinterpret_p64_u64(a), 0), \
		vget_lane_p64(vreinterpret_p64_u64(b), 0)))

static __m128i _mm_set_epi32(int i3, int i2, int i1, int i0)
{
	int32_t __attribute__((aligned(16))) data[4] = {i0, i1, i2, i3};

	return vreinterpretq_s64_s32(vld1q_s32(data));
}

static __m128i _mm_set_epi8(int8_t b15, int8_t b14, int8_t b13, int8_t b12, int8_t b11,
	int8_t b10, int8_t b9, int8_t b8, int8_t b7, int8_t b6, int8_t b5, int8_t b4,
	int8_t b3, int8_t b2, int8_t b1, int8_t b0)
{
	int8_t __attribute__((aligned(16))) data[16] =
		{b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15};

	return (__m128i) vld1q_s8(data);
}

static __m128i _mm_aeskeygenassist_si128(__m128i a, const int rcon)
{
	uint8x16_t u8 = vaeseq_u8(vreinterpretq_u8_s64(a), vdupq_n_u8(0));

	uint8x16_t dest = {
		u8[0x4], u8[0x1], u8[0xE], u8[0xB],
		u8[0x1], u8[0xE], u8[0xB], u8[0x4],
		u8[0xC], u8[0x9], u8[0x6], u8[0x3],
		u8[0x9], u8[0x6], u8[0x3], u8[0xC],
	};

	uint32x4_t r = {0, (uint32_t) rcon, 0, (uint32_t) rcon};

	return vreinterpretq_s64_u8(dest) ^ vreinterpretq_s64_u32(r);
}

static __m128i _mm_clmulepi64_si128(__m128i _a, __m128i _b, const int imm)
{
	uint64x2_t a = vreinterpretq_u64_s64(_a);
	uint64x2_t b = vreinterpretq_u64_s64(_b);

	switch (imm & 0x11) {
		case 0x00:
			return vreinterpretq_s64_u64(_vmull_p64(vget_low_u64(a), vget_low_u64(b)));
		case 0x01:
			return vreinterpretq_s64_u64(_vmull_p64(vget_high_u64(a), vget_low_u64(b)));
		case 0x10:
			return vreinterpretq_s64_u64(_vmull_p64(vget_low_u64(a), vget_high_u64(b)));
		case 0x11:
			return vreinterpretq_s64_u64(_vmull_p64(vget_high_u64(a), vget_high_u64(b)));
		default:
			abort();
	}
}
