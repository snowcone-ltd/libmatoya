// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <aaudio/AAudio.h>

#define AUDIO_SAMPLE_SIZE sizeof(int16_t)

#define AUDIO_BUF_SIZE(ctx) \
	((ctx)->sample_rate * (ctx)->channels * AUDIO_SAMPLE_SIZE)

struct MTY_Audio {
	AAudioStreamBuilder *builder;
	AAudioStream *stream;

	MTY_Mutex *mutex;
	uint32_t sample_rate;
	uint32_t min_buffer;
	uint32_t max_buffer;
	uint8_t channels;
	bool flushing;
	bool playing;

	uint8_t *buffer;
	size_t size;
	size_t min_request;
};

static void audio_error(AAudioStream *stream, void *userData, aaudio_result_t error)
{
	MTY_Log("'AAudioStream' error %d", error);
}

static aaudio_data_callback_result_t audio_callback(AAudioStream *stream, void *userData,
	void *audioData, int32_t numFrames)
{
	MTY_Audio *ctx = userData;

	MTY_MutexLock(ctx->mutex);

	size_t want_size = numFrames * ctx->channels * AUDIO_SAMPLE_SIZE;

	if (ctx->playing && ctx->size >= want_size) {
		memcpy(audioData, ctx->buffer, want_size);
		ctx->size -= want_size;
		ctx->min_request = MTY_MIN(want_size, ctx->min_request);

		memmove(ctx->buffer, ctx->buffer + want_size, ctx->size);

	} else {
		memset(audioData, 0, want_size);
	}

	MTY_MutexUnlock(ctx->mutex);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

MTY_Audio *MTY_AudioCreate(uint32_t sampleRate, uint32_t minBuffer, uint32_t maxBuffer, uint8_t channels,
	const char *deviceID, bool fallback)
{
	MTY_Audio *ctx = MTY_Alloc(1, sizeof(MTY_Audio));
	ctx->channels = channels;
	ctx->sample_rate = sampleRate;
	ctx->mutex = MTY_MutexCreate();
	ctx->buffer = MTY_Alloc(AUDIO_BUF_SIZE(ctx), 1);

	uint32_t frames_per_ms = lrint((float) sampleRate / 1000.0f);
	ctx->min_buffer = minBuffer * frames_per_ms * ctx->channels * AUDIO_SAMPLE_SIZE;
	ctx->max_buffer = maxBuffer * frames_per_ms * ctx->channels * AUDIO_SAMPLE_SIZE;
	ctx->min_request = ctx->max_buffer;

	return ctx;
}

void MTY_AudioDestroy(MTY_Audio **audio)
{
	if (!audio || !*audio)
		return;

	MTY_Audio *ctx = *audio;

	MTY_AudioReset(ctx);

	MTY_MutexDestroy(&ctx->mutex);
	MTY_Free(ctx->buffer);

	MTY_Free(ctx);
	*audio = NULL;
}

void MTY_AudioReset(MTY_Audio *ctx)
{
	if (ctx->stream) {
		MTY_MutexLock(ctx->mutex);

		ctx->playing = false;
		ctx->flushing = false;
		ctx->size = 0;
		ctx->min_request = ctx->max_buffer;

		MTY_MutexUnlock(ctx->mutex);

		AAudioStream_requestStop(ctx->stream);
		AAudioStream_close(ctx->stream);
		ctx->stream = NULL;
	}

	if (ctx->builder) {
		AAudioStreamBuilder_delete(ctx->builder);
		ctx->builder = NULL;
	}
}

uint32_t MTY_AudioGetQueued(MTY_Audio *ctx)
{
	return (ctx->size / (ctx->channels * AUDIO_SAMPLE_SIZE)) / ctx->sample_rate * 1000;
}

static void audio_start(MTY_Audio *ctx)
{
	if (!ctx->builder) {
		AAudio_createStreamBuilder(&ctx->builder);
		AAudioStreamBuilder_setDeviceId(ctx->builder, AAUDIO_UNSPECIFIED);
		AAudioStreamBuilder_setSampleRate(ctx->builder, ctx->sample_rate);
		AAudioStreamBuilder_setChannelCount(ctx->builder, ctx->channels);
		AAudioStreamBuilder_setFormat(ctx->builder, AAUDIO_FORMAT_PCM_I16);
		AAudioStreamBuilder_setPerformanceMode(ctx->builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
		AAudioStreamBuilder_setErrorCallback(ctx->builder, audio_error, ctx);
		AAudioStreamBuilder_setDataCallback(ctx->builder, audio_callback, ctx);
	}

	if (!ctx->stream) {
		AAudioStreamBuilder_openStream(ctx->builder, &ctx->stream);
		AAudioStream_requestStart(ctx->stream);
	}
}

void MTY_AudioQueue(MTY_Audio *ctx, const int16_t *frames, uint32_t count)
{
	size_t data_size = count * ctx->channels * AUDIO_SAMPLE_SIZE;

	audio_start(ctx);

	MTY_MutexLock(ctx->mutex);

	if (ctx->size + data_size >= ctx->max_buffer)
		ctx->flushing = true;

	// If the data remaining is less than the minimum Android has ever requested, clear it so we don't get stuck flushing
	if (ctx->flushing && ctx->size < ctx->min_request) {
		memset(ctx->buffer, 0, ctx->size);
		ctx->size = 0;
	}

	if (ctx->size == 0) {
		ctx->playing = false;
		ctx->flushing = false;
	}

	if (!ctx->flushing && data_size + ctx->size <= AUDIO_BUF_SIZE(ctx)) {
		memcpy(ctx->buffer + ctx->size, frames, data_size);
		ctx->size += data_size;
	}

	if (ctx->size >= ctx->min_buffer)
		ctx->playing = true;

	MTY_MutexUnlock(ctx->mutex);
}
