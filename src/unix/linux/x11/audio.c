// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>
#include <math.h>

#include "dl/libasound.h"

#define AUDIO_CHANNELS    2
#define AUDIO_SAMPLE_SIZE sizeof(int16_t)
#define AUDIO_BUF_SIZE    (48000 * AUDIO_CHANNELS * AUDIO_SAMPLE_SIZE)

struct MTY_Audio {
	snd_pcm_t *pcm;

	bool playing;
	uint32_t sample_rate;
	uint32_t min_buffer;
	uint32_t max_buffer;
	uint8_t *buf;
	size_t pos;
};

MTY_Audio *MTY_AudioCreate(uint32_t sampleRate, uint32_t minBuffer, uint32_t maxBuffer, uint8_t channels,
	const char *deviceID, bool fallback)
{
	if (!libasound_global_init())
		return NULL;

	MTY_Audio *ctx = MTY_Alloc(1, sizeof(MTY_Audio));
	ctx->sample_rate = sampleRate;

	uint32_t frames_per_ms = lrint((float) sampleRate / 1000.0f);
	ctx->min_buffer = minBuffer * frames_per_ms;
	ctx->max_buffer = maxBuffer * frames_per_ms;

	bool r = true;

	int32_t e = snd_pcm_open(&ctx->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (e != 0) {
		MTY_Log("'snd_pcm_open' failed with error %d", e);
		r = false;
		goto except;
	}

	snd_pcm_hw_params_t *params = NULL;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(ctx->pcm, params);

	snd_pcm_hw_params_set_access(ctx->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(ctx->pcm, params, SND_PCM_FORMAT_S16);
	snd_pcm_hw_params_set_channels(ctx->pcm, params, AUDIO_CHANNELS);
	snd_pcm_hw_params_set_rate(ctx->pcm, params, sampleRate, 0);
	snd_pcm_hw_params(ctx->pcm, params);
	snd_pcm_nonblock(ctx->pcm, 1);

	ctx->buf = MTY_Alloc(AUDIO_BUF_SIZE, 1);

	except:

	if (!r)
		MTY_AudioDestroy(&ctx);

	return ctx;
}

void MTY_AudioDestroy(MTY_Audio **audio)
{
	if (!audio || !*audio)
		return;

	MTY_Audio *ctx = *audio;

	if (ctx->pcm)
		snd_pcm_close(ctx->pcm);

	MTY_Free(ctx->buf);
	MTY_Free(ctx);
	*audio = NULL;
}

static uint32_t audio_get_queued_frames(MTY_Audio *ctx)
{
	uint32_t queued = ctx->pos / 4;

	if (ctx->playing) {
		snd_pcm_status_t *status = NULL;
		snd_pcm_status_alloca(&status);

		if (snd_pcm_status(ctx->pcm, status) >= 0) {
			snd_pcm_uframes_t avail = snd_pcm_status_get_avail(status);
			snd_pcm_uframes_t avail_max = snd_pcm_status_get_avail_max(status);

			if (avail_max > 0)
				queued += (int32_t) avail_max - (int32_t) avail;
		}
	}

	return queued;
}

static void audio_play(MTY_Audio *ctx)
{
	if (!ctx->playing) {
		snd_pcm_prepare(ctx->pcm);
		ctx->playing = true;
	}
}

void MTY_AudioReset(MTY_Audio *ctx)
{
	ctx->playing = false;
	ctx->pos = 0;
}

uint32_t MTY_AudioGetQueued(MTY_Audio *ctx)
{
	return lrint((float) audio_get_queued_frames(ctx) / ((float) ctx->sample_rate / 1000.0f));
}

void MTY_AudioQueue(MTY_Audio *ctx, const int16_t *frames, uint32_t count)
{
	size_t size = count * 4;

	uint32_t queued = audio_get_queued_frames(ctx);

	// Stop playing and flush if we've exceeded the maximum buffer or underrun
	if (ctx->playing && (queued > ctx->max_buffer || queued == 0))
		MTY_AudioReset(ctx);

	if (ctx->pos + size <= AUDIO_BUF_SIZE) {
		memcpy(ctx->buf + ctx->pos, frames, count * 4);
		ctx->pos += size;
	}

	// Begin playing again when the minimum buffer has been reached
	if (!ctx->playing && queued + count >= ctx->min_buffer)
		audio_play(ctx);

	if (ctx->playing) {
		int32_t e = snd_pcm_writei(ctx->pcm, ctx->buf, ctx->pos / 4);

		if (e >= 0) {
			ctx->pos = 0;

		} else if (e == -EPIPE) {
			MTY_AudioReset(ctx);
		}
	}
}
