// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <alloca.h>
#include <errno.h>

#include "sym.h"


// Interface

typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;
typedef struct _snd_pcm_status snd_pcm_status_t;
typedef unsigned long snd_pcm_uframes_t;
typedef long snd_pcm_sframes_t;

typedef enum _snd_pcm_stream {
	SND_PCM_STREAM_PLAYBACK = 0,
	SND_PCM_STREAM_CAPTURE,
	SND_PCM_STREAM_LAST = SND_PCM_STREAM_CAPTURE
} snd_pcm_stream_t;

typedef enum _snd_pcm_access {
	SND_PCM_ACCESS_MMAP_INTERLEAVED = 0,
	SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
	SND_PCM_ACCESS_MMAP_COMPLEX,
	SND_PCM_ACCESS_RW_INTERLEAVED,
	SND_PCM_ACCESS_RW_NONINTERLEAVED,
	SND_PCM_ACCESS_LAST = SND_PCM_ACCESS_RW_NONINTERLEAVED
} snd_pcm_access_t;

typedef enum _snd_pcm_format {
	SND_PCM_FORMAT_UNKNOWN = -1,
	SND_PCM_FORMAT_S8 = 0,
	SND_PCM_FORMAT_U8,
	SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_S16_BE,
	SND_PCM_FORMAT_U16_LE,
	SND_PCM_FORMAT_U16_BE,
	SND_PCM_FORMAT_S24_LE,
	SND_PCM_FORMAT_S24_BE,
	SND_PCM_FORMAT_U24_LE,
	SND_PCM_FORMAT_U24_BE,
	SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_S32_BE,
	SND_PCM_FORMAT_U32_LE,
	SND_PCM_FORMAT_U32_BE,
	SND_PCM_FORMAT_FLOAT_LE,
	SND_PCM_FORMAT_FLOAT_BE,
	SND_PCM_FORMAT_FLOAT64_LE,
	SND_PCM_FORMAT_FLOAT64_BE,
	SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
	SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
	SND_PCM_FORMAT_MU_LAW,
	SND_PCM_FORMAT_A_LAW,
	SND_PCM_FORMAT_IMA_ADPCM,
	SND_PCM_FORMAT_MPEG,
	SND_PCM_FORMAT_GSM,
	SND_PCM_FORMAT_S20_LE,
	SND_PCM_FORMAT_S20_BE,
	SND_PCM_FORMAT_U20_LE,
	SND_PCM_FORMAT_U20_BE,
	SND_PCM_FORMAT_SPECIAL = 31,
	SND_PCM_FORMAT_S24_3LE = 32,
	SND_PCM_FORMAT_S24_3BE,
	SND_PCM_FORMAT_U24_3LE,
	SND_PCM_FORMAT_U24_3BE,
	SND_PCM_FORMAT_S20_3LE,
	SND_PCM_FORMAT_S20_3BE,
	SND_PCM_FORMAT_U20_3LE,
	SND_PCM_FORMAT_U20_3BE,
	SND_PCM_FORMAT_S18_3LE,
	SND_PCM_FORMAT_S18_3BE,
	SND_PCM_FORMAT_U18_3LE,
	SND_PCM_FORMAT_U18_3BE,
	/* G.723 (ADPCM) 24 kbit/s, 8 samples in 3 bytes */
	SND_PCM_FORMAT_G723_24,
	/* G.723 (ADPCM) 24 kbit/s, 1 sample in 1 byte */
	SND_PCM_FORMAT_G723_24_1B,
	/* G.723 (ADPCM) 40 kbit/s, 8 samples in 3 bytes */
	SND_PCM_FORMAT_G723_40,
	/* G.723 (ADPCM) 40 kbit/s, 1 sample in 1 byte */
	SND_PCM_FORMAT_G723_40_1B,
	/* Direct Stream Digital (DSD) in 1-byte samples (x8) */
	SND_PCM_FORMAT_DSD_U8,
	/* Direct Stream Digital (DSD) in 2-byte samples (x16) */
	SND_PCM_FORMAT_DSD_U16_LE,
	/* Direct Stream Digital (DSD) in 4-byte samples (x32) */
	SND_PCM_FORMAT_DSD_U32_LE,
	/* Direct Stream Digital (DSD) in 2-byte samples (x16) */
	SND_PCM_FORMAT_DSD_U16_BE,
	/* Direct Stream Digital (DSD) in 4-byte samples (x32) */
	SND_PCM_FORMAT_DSD_U32_BE,
	SND_PCM_FORMAT_LAST = SND_PCM_FORMAT_DSD_U32_BE,

	#if __BYTE_ORDER == __LITTLE_ENDIAN

	SND_PCM_FORMAT_S16 = SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_U16 = SND_PCM_FORMAT_U16_LE,
	SND_PCM_FORMAT_S24 = SND_PCM_FORMAT_S24_LE,
	SND_PCM_FORMAT_U24 = SND_PCM_FORMAT_U24_LE,
	SND_PCM_FORMAT_S32 = SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_U32 = SND_PCM_FORMAT_U32_LE,
	SND_PCM_FORMAT_FLOAT = SND_PCM_FORMAT_FLOAT_LE,
	SND_PCM_FORMAT_FLOAT64 = SND_PCM_FORMAT_FLOAT64_LE,
	SND_PCM_FORMAT_IEC958_SUBFRAME = SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
	SND_PCM_FORMAT_S20 = SND_PCM_FORMAT_S20_LE,
	SND_PCM_FORMAT_U20 = SND_PCM_FORMAT_U20_LE,
	#elif __BYTE_ORDER == __BIG_ENDIAN

	SND_PCM_FORMAT_S16 = SND_PCM_FORMAT_S16_BE,
	SND_PCM_FORMAT_U16 = SND_PCM_FORMAT_U16_BE,
	SND_PCM_FORMAT_S24 = SND_PCM_FORMAT_S24_BE,
	SND_PCM_FORMAT_U24 = SND_PCM_FORMAT_U24_BE,
	SND_PCM_FORMAT_S32 = SND_PCM_FORMAT_S32_BE,
	SND_PCM_FORMAT_U32 = SND_PCM_FORMAT_U32_BE,
	SND_PCM_FORMAT_FLOAT = SND_PCM_FORMAT_FLOAT_BE,
	SND_PCM_FORMAT_FLOAT64 = SND_PCM_FORMAT_FLOAT64_BE,
	SND_PCM_FORMAT_IEC958_SUBFRAME = SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
	SND_PCM_FORMAT_S20 = SND_PCM_FORMAT_S20_BE,
	SND_PCM_FORMAT_U20 = SND_PCM_FORMAT_U20_BE,
	#else
	#error "Unknown endian"
	#endif
} snd_pcm_format_t;

#define	__snd_alloca(ptr, type)       do {*ptr = (type##_t *) alloca(type##_sizeof()); memset(*ptr, 0, type##_sizeof());} while (0)
#define snd_pcm_status_alloca(ptr)    __snd_alloca(ptr, snd_pcm_status)
#define snd_pcm_hw_params_alloca(ptr) __snd_alloca(ptr, snd_pcm_hw_params)

static int (*snd_pcm_open)(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode);
static int (*snd_pcm_hw_params_any)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
static int (*snd_pcm_hw_params_set_access)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t _access);
static int (*snd_pcm_hw_params_set_format)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val);
static int (*snd_pcm_hw_params_set_channels)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
static int (*snd_pcm_hw_params_set_rate)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
static int (*snd_pcm_hw_params)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
static int (*snd_pcm_prepare)(snd_pcm_t *pcm);
static snd_pcm_sframes_t (*snd_pcm_writei)(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
static int (*snd_pcm_close)(snd_pcm_t *pcm);
static int (*snd_pcm_nonblock)(snd_pcm_t *pcm, int nonblock);
static int (*snd_pcm_status)(snd_pcm_t *pcm, snd_pcm_status_t *status);
static size_t (*snd_pcm_status_sizeof)(void);
static size_t (*snd_pcm_hw_params_sizeof)(void);
static snd_pcm_uframes_t (*snd_pcm_status_get_avail)(const snd_pcm_status_t *obj);
static snd_pcm_uframes_t (*snd_pcm_status_get_avail_max)(const snd_pcm_status_t *obj);


// Runtime open

static MTY_Atomic32 LIBASOUND_LOCK;
static MTY_SO *LIBASOUND_SO;
static bool LIBASOUND_INIT;

static void __attribute__((destructor)) libasound_global_destroy(void)
{
	MTY_GlobalLock(&LIBASOUND_LOCK);

	MTY_SOUnload(&LIBASOUND_SO);
	LIBASOUND_INIT = false;

	MTY_GlobalUnlock(&LIBASOUND_LOCK);
}

static bool libasound_global_init(void)
{
	MTY_GlobalLock(&LIBASOUND_LOCK);

	if (!LIBASOUND_INIT) {
		bool r = true;

		LIBASOUND_SO = MTY_SOLoad("libasound.so.2");

		if (!LIBASOUND_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBASOUND_SO, snd_pcm_open);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_any);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_set_access);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_set_format);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_set_channels);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_set_rate);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_prepare);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_writei);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_close);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_nonblock);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_status);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_status_sizeof);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_sizeof);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_status_get_avail);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_status_get_avail_max);

		except:

		if (!r)
			libasound_global_destroy();

		LIBASOUND_INIT = r;
	}

	MTY_GlobalUnlock(&LIBASOUND_LOCK);

	return LIBASOUND_INIT;
}
