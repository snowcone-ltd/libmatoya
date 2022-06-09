// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "resample-sinc.h"

#define RESAMPLE_BUF_LEN (512 * 1024)
#define RESAMPLE_BITS    12

struct MTY_Resampler {
	size_t len;
	double index;
	double ratio;
	int16_t buffer[RESAMPLE_BUF_LEN];
	int16_t out[RESAMPLE_BUF_LEN];
};

MTY_Resampler *MTY_ResamplerCreate(void)
{
	return MTY_Alloc(1, sizeof(MTY_Resampler));
}

void MTY_ResamplerDestroy(MTY_Resampler **resampler)
{
	if (!resampler || !*resampler)
		return;

	MTY_Resampler *ctx = *resampler;

	MTY_Free(ctx);
	*resampler = NULL;
}

static int16_t resample_float_to_int16(float in)
{
	return in > INT16_MAX ? INT16_MAX : in < INT16_MIN ? INT16_MIN : (int16_t) lrintf(in);
}

static void resample_output(bool left, const int16_t *buffer, size_t pos, double ratio,
	double index, double *out)
{
	double finc = RESAMPLE_INC * (ratio < 1.0 ? ratio : 1.0);
	double scale = finc / RESAMPLE_INC;

	size_t inc = lrint(finc * (1 << RESAMPLE_BITS));
	size_t start = lrint(index * finc * (1 << RESAMPLE_BITS));
	size_t cindex = left ? start : inc - start;
	size_t count = ((RESAMPLE_HALF_LEN << RESAMPLE_BITS) - cindex) / inc;
	size_t dindex = left ? pos - 2 * count : pos + 2 * (1 + count);

	cindex += count * inc;

	while (true) {
		double frac = (cindex & ((1 << RESAMPLE_BITS) - 1)) * (1.0 / (1 << RESAMPLE_BITS));
		size_t i = cindex >> RESAMPLE_BITS;

		double sinc = RESAMPLE_SINC[i] + frac * (RESAMPLE_SINC[i + 1] - RESAMPLE_SINC[i]);

		out[0] += sinc * scale * buffer[dindex];
		out[1] += sinc * scale * buffer[dindex + 1];

		if (inc > cindex)
			break;

		if (left && inc == cindex)
			break;

		cindex -= inc;
		dindex += left ? 2 : -2;
	}
}

const int16_t *MTY_Resample(MTY_Resampler *ctx, double ratio, const int16_t *in, size_t inFrames,
	size_t *outFrames)
{
	if (ctx->ratio == 0.0)
		ctx->ratio = ratio;

	double count = (RESAMPLE_HALF_LEN + 2.0) / RESAMPLE_INC;
	double min_ratio = ctx->ratio < ratio ? ctx->ratio : ratio;
	if (min_ratio < 1.0)
		count /= min_ratio;

	size_t half_len = 2 * (lrint(count) + 1);
	size_t pos = half_len;

	if (ctx->len == 0)
		ctx->len = half_len;

	memcpy(ctx->buffer + ctx->len, in, inFrames * 2 * sizeof(int16_t));
	ctx->len += inFrames * 2;

	double cur_ratio = ctx->ratio;

	for (*outFrames = 0; *outFrames < RESAMPLE_BUF_LEN; *outFrames += 1) {
		size_t have = ctx->len - pos;

		if (have <= half_len) {
			ctx->len = have + half_len;
			memmove(ctx->buffer, ctx->buffer + pos - half_len, ctx->len * sizeof(int16_t));
			break;
		}

		if (fabs(ctx->ratio - ratio) > 0.0000000001)
			cur_ratio = ctx->ratio + *outFrames * 2 * (ratio - ctx->ratio) / RESAMPLE_BUF_LEN;

		double channels[2] = {0};
		resample_output(true, ctx->buffer, pos, cur_ratio, ctx->index, channels);
		resample_output(false, ctx->buffer, pos, cur_ratio, ctx->index, channels);

		ctx->out[*outFrames * 2] = resample_float_to_int16((float) channels[0]);
		ctx->out[*outFrames * 2 + 1] = resample_float_to_int16((float) channels[1]);

		ctx->index += 1.0 / cur_ratio;

		double rem = fmod(ctx->index, 1.0);
		pos += 2 * lrint(ctx->index - rem);
		ctx->index = rem;
	}

	ctx->ratio = cur_ratio;

	return ctx->out;
}

void MTY_ResamplerReset(MTY_Resampler *ctx)
{
	memset(ctx, 0, sizeof(MTY_Resampler));
}
