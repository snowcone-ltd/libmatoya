// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <AudioToolbox/AudioToolbox.h>

#define AUDIO_SAMPLE_SIZE sizeof(int16_t)
#define AUDIO_BUFS        64

#define AUDIO_BUF_SIZE(ctx) \
	((ctx)->sample_rate * (ctx)->channels * AUDIO_SAMPLE_SIZE)

struct MTY_Audio {
	AudioQueueRef q;
	AudioQueueBufferRef audio_buf[AUDIO_BUFS];
	MTY_Atomic32 in_use[AUDIO_BUFS];
	uint32_t sample_rate;
	uint32_t min_buffer;
	uint32_t max_buffer;
	uint8_t channels;
	bool playing;
};

static void audio_queue_callback(void *opaque, AudioQueueRef q, AudioQueueBufferRef buf)
{
	MTY_Audio *ctx = opaque;

	MTY_Atomic32Set(&ctx->in_use[(uintptr_t) buf->mUserData], 0);
	buf->mAudioDataByteSize = 0;
}

MTY_Audio *MTY_AudioCreate(uint32_t sampleRate, uint32_t minBuffer, uint32_t maxBuffer, uint8_t channels,
	const char *deviceID, bool fallback)
{
	// TODO Should this use the current run loop rather than internal threading?

	MTY_Audio *ctx = MTY_Alloc(1, sizeof(MTY_Audio));
	ctx->sample_rate = sampleRate;
	ctx->channels = channels;

	uint32_t frames_per_ms = lrint((float) sampleRate / 1000.0f);
	ctx->min_buffer = minBuffer * frames_per_ms;
	ctx->max_buffer = maxBuffer * frames_per_ms;

	AudioStreamBasicDescription format = {0};
	format.mSampleRate = sampleRate;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	format.mFramesPerPacket = 1;
	format.mChannelsPerFrame = channels;
	format.mBitsPerChannel = AUDIO_SAMPLE_SIZE * 8;
	format.mBytesPerPacket = AUDIO_SAMPLE_SIZE * format.mChannelsPerFrame;
	format.mBytesPerFrame = format.mBytesPerPacket;

	OSStatus e = AudioQueueNewOutput(&format, audio_queue_callback, ctx, NULL, NULL, 0, &ctx->q);
	if (e != kAudioServicesNoError) {
		MTY_Log("'AudioQueueNewOutput' failed with error 0x%X", e);
		goto except;
	}

	for (int32_t x = 0; x < AUDIO_BUFS; x++) {
		e = AudioQueueAllocateBuffer(ctx->q, AUDIO_BUF_SIZE(ctx), &ctx->audio_buf[x]);
		if (e != kAudioServicesNoError) {
			MTY_Log("'AudioQueueAllocateBuffer' failed with error 0x%X", e);
			goto except;
		}
	}

	except:

	if (e != kAudioServicesNoError)
		MTY_AudioDestroy(&ctx);

	return ctx;
}

void MTY_AudioDestroy(MTY_Audio **audio)
{
	if (!audio || !*audio)
		return;

	MTY_Audio *ctx = *audio;

	if (ctx->q) {
		OSStatus e = AudioQueueDispose(ctx->q, true);
		if (e != kAudioServicesNoError)
			MTY_Log("'AudioQueueDispose' failed with error 0x%X", e);
	}

	MTY_Free(ctx);
	*audio = NULL;
}

static uint32_t audio_get_queued_frames(MTY_Audio *ctx)
{
	size_t queued = 0;

	for (uint8_t x = 0; x < AUDIO_BUFS; x++) {
		if (MTY_Atomic32Get(&ctx->in_use[x]) == 1) {
			AudioQueueBufferRef buf = ctx->audio_buf[x];
			queued += buf->mAudioDataByteSize;
		}
	}

	return queued / (ctx->channels * AUDIO_SAMPLE_SIZE);
}

static void audio_play(MTY_Audio *ctx)
{
	if (ctx->playing)
		return;

	if (AudioQueueStart(ctx->q, NULL) == kAudioServicesNoError)
		ctx->playing = true;
}

void MTY_AudioReset(MTY_Audio *ctx)
{
	if (!ctx->playing)
		return;

	if (AudioQueueStop(ctx->q, true) == kAudioServicesNoError)
		ctx->playing = false;
}

uint32_t MTY_AudioGetQueued(MTY_Audio *ctx)
{
	return lrint((float) audio_get_queued_frames(ctx) / ((float) ctx->sample_rate / 1000.0f));
}

void MTY_AudioQueue(MTY_Audio *ctx, const int16_t *frames, uint32_t count)
{
	size_t size = count * ctx->channels * AUDIO_SAMPLE_SIZE;
	uint32_t queued = audio_get_queued_frames(ctx);

	// Stop playing and flush if we've exceeded the maximum buffer or underrun
	if (ctx->playing && (queued > ctx->max_buffer || queued == 0))
		MTY_AudioReset(ctx);

	if (size <= AUDIO_BUF_SIZE(ctx)) {
		for (uint8_t x = 0; x < AUDIO_BUFS; x++) {
			if (MTY_Atomic32Get(&ctx->in_use[x]) == 0) {
				AudioQueueBufferRef buf = ctx->audio_buf[x];

				memcpy(buf->mAudioData, frames, size);
				buf->mAudioDataByteSize = size;
				buf->mUserData = (void *) (uintptr_t) x;

				OSStatus e = AudioQueueEnqueueBuffer(ctx->q, buf, 0, NULL);
				if (e == kAudioServicesNoError) {
					MTY_Atomic32Set(&ctx->in_use[x], 1);

				} else {
					MTY_Log("'AudioQueueEnqueueBuffer' failed with error 0x%X", e);
				}
				break;
			}
		}

		// Begin playing again when the minimum buffer has been reached
		if (!ctx->playing && queued + count >= ctx->min_buffer)
			audio_play(ctx);
	}
}
