#define STBI_VERSION 1

enum {
	STBI_default = 0, // only used for desired_channels

	STBI_grey = 1,
	STBI_grey_alpha = 2,
	STBI_rgb = 3,
	STBI_rgb_alpha = 4
};

typedef unsigned char stbi_uc;
typedef unsigned short stbi_us;

//////////////////////////////////////////////////////////////////////////////
//
// PRIMARY API - works on images of any type
//

//
// load image by filename, open file, or memory buffer
//

typedef struct {
	int (*read)(void *user, char *data, int size); // fill 'data' with 'size' bytes.  return number of bytes actually read
	void (*skip)(void *user, int n);               // skip the next 'n' bytes, or 'unget' the last -n bytes if negative
	int (*eof)(void *user);                        // returns nonzero if we are at end of file/data
} stbi_io_callbacks;

////////////////////////////////////
//
// 8-bits-per-channel interface
//


#if defined(STBI_NO_PNG) && !defined(STBI_SUPPORT_ZLIB) && !defined(STBI_NO_ZLIB)
	#define STBI_NO_ZLIB
#endif

#if defined(_WIN32)
	#define STBI_THREAD_LOCAL __declspec(thread)
#else
	#define STBI_THREAD_LOCAL __thread
#endif

typedef uint16_t stbi__uint16;
typedef int16_t stbi__int16;
typedef uint32_t stbi__uint32;
typedef int32_t stbi__int32;

// should produce compiler error if size is wrong
typedef unsigned char validate_uint32[sizeof(stbi__uint32) == 4 ? 1 : -1];

#ifdef _MSC_VER
	#define STBI_NOTUSED(v) (void) (v)
#else
	#define STBI_NOTUSED(v) (void) sizeof(v)
#endif

#ifdef _MSC_VER
	#define stbi_lrot(x, y) _lrotl(x, y)
#else
	#define stbi_lrot(x, y) (((x) << (y)) | ((x) >> (32 - (y))))
#endif

#ifndef STBI_REALLOC_SIZED
	#define STBI_REALLOC_SIZED(p, oldsz, newsz) STBI_REALLOC(p, newsz)
#endif

// x86/x64 detection
#if defined(__x86_64__) || defined(_M_X64)
	#define STBI__X64_TARGET
#elif defined(__i386) || defined(_M_IX86)
	#define STBI__X86_TARGET
#endif

#if defined(__GNUC__) && defined(STBI__X86_TARGET) && !defined(__SSE2__) && !defined(STBI_NO_SIMD)
	// gcc doesn't support sse2 intrinsics unless you compile with -msse2,
	// which in turn means it gets to use SSE2 everywhere. This is unfortunate,
	// but previous attempts to provide the SSE2 functions with runtime
	// detection caused numerous issues. The way architecture extensions are
	// exposed in GCC/Clang is, sadly, not really suited for one-file libs.
	// New behavior: if compiled with -msse2, we use SSE2 without any
	// detection; if not, we don't use it at all.
	#define STBI_NO_SIMD
#endif

#if defined(__MINGW32__) && defined(STBI__X86_TARGET) && !defined(STBI_MINGW_ENABLE_SSE2) && !defined(STBI_NO_SIMD)
	// Note that __MINGW32__ doesn't actually mean 32-bit, so we have to avoid STBI__X64_TARGET
	//
	// 32-bit MinGW wants ESP to be 16-byte aligned, but this is not in the
	// Windows ABI and VC++ as well as Windows DLLs don't maintain that invariant.
	// As a result, enabling SSE2 on 32-bit MinGW is dangerous when not
	// simultaneously enabling "-mstackrealign".
	//
	// See https://github.com/nothings/stb/issues/81 for more information.
	//
	// So default to no SSE2 on 32-bit MinGW. If you've read this far and added
	// -mstackrealign to your build settings, feel free to #define STBI_MINGW_ENABLE_SSE2.
	#define STBI_NO_SIMD
#endif

#if !defined(STBI_NO_SIMD) && (defined(STBI__X86_TARGET) || defined(STBI__X64_TARGET))
	#define STBI_SSE2

	#ifdef _MSC_VER

static int stbi__cpuid3(void)
{
	int info[4];
	__cpuid(info, 1);
	return info[3];
}

		#define STBI_SIMD_ALIGN(type, name) __declspec(align(16)) type name

		#if !defined(STBI_NO_JPEG) && defined(STBI_SSE2)
static int stbi__sse2_available(void)
{
	int info3 = stbi__cpuid3();
	return ((info3 >> 26) & 1) != 0;
}
		#endif

	#else // assume GCC-style if not VC++
		#define STBI_SIMD_ALIGN(type, name) type name __attribute__((aligned(16)))

		#if !defined(STBI_NO_JPEG) && defined(STBI_SSE2)
static int stbi__sse2_available(void)
{
	// If we're even attempting to compile this on GCC/Clang, that means
	// -msse2 is on, which means the compiler is allowed to use SSE2
	// instructions at will, and so are we.
	return 1;
}
		#endif

	#endif
#endif

// ARM NEON
#if defined(STBI_NO_SIMD) && defined(STBI_NEON)
	#undef STBI_NEON
#endif

#ifdef STBI_NEON
	// assume GCC or Clang on ARM targets
	#define STBI_SIMD_ALIGN(type, name) type name __attribute__((aligned(16)))
#endif

#ifndef STBI_SIMD_ALIGN
	#define STBI_SIMD_ALIGN(type, name) type name
#endif

#ifndef STBI_MAX_DIMENSIONS
	#define STBI_MAX_DIMENSIONS (1 << 24)
#endif

///////////////////////////////////////////////
//
//  stbi__context struct and start_xxx functions

// stbi__context structure is our basic context used by all images, so it
// contains all the IO context, plus some basic image information
typedef struct {
	stbi__uint32 img_x, img_y;
	int img_n, img_out_n;

	stbi_io_callbacks io;
	void *io_user_data;

	int read_from_callbacks;
	int buflen;
	stbi_uc buffer_start[128];
	int callback_already_read;

	stbi_uc *img_buffer, *img_buffer_end;
	stbi_uc *img_buffer_original, *img_buffer_original_end;
} stbi__context;


static void stbi__refill_buffer(stbi__context *s);

// initialize a memory-decode context
static void stbi__start_mem(stbi__context *s, stbi_uc const *buffer, int len)
{
	s->io.read = NULL;
	s->read_from_callbacks = 0;
	s->callback_already_read = 0;
	s->img_buffer = s->img_buffer_original = (stbi_uc *) buffer;
	s->img_buffer_end = s->img_buffer_original_end = (stbi_uc *) buffer + len;
}

static void stbi__rewind(stbi__context *s)
{
	// conceptually rewind SHOULD rewind to the beginning of the stream,
	// but we just rewind to the beginning of the initial buffer, because
	// we only use it after doing 'test', which only ever looks at at most 92 bytes
	s->img_buffer = s->img_buffer_original;
	s->img_buffer_end = s->img_buffer_original_end;
}

enum { STBI_ORDER_RGB, STBI_ORDER_BGR };

typedef struct {
	int bits_per_channel;
	int num_channels;
	int channel_order;
} stbi__result_info;

#ifndef STBI_NO_JPEG
static int stbi__jpeg_test(stbi__context *s);
static void *stbi__jpeg_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
#endif

#ifndef STBI_NO_PNG
static int stbi__png_test(stbi__context *s);
static void *stbi__png_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
#endif

#ifndef STBI_NO_GIF
static int stbi__gif_test(stbi__context *s);
static void *stbi__gif_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
#endif

static
#ifdef STBI_THREAD_LOCAL
	STBI_THREAD_LOCAL
#endif
	const char *stbi__g_failure_reason;

#ifndef STBI_NO_FAILURE_STRINGS
static int stbi__err(const char *str)
{
	stbi__g_failure_reason = str;
	return 0;
}
#endif

static void *stbi__malloc(size_t size)
{
	return STBI_MALLOC(size);
}

// stb_image uses ints pervasively, including for offset calculations.
// therefore the largest decoded image size we can support with the
// current code, even on 64-bit targets, is INT_MAX. this is not a
// significant limitation for the intended use case.
//
// we do, however, need to make sure our size calculations don't
// overflow. hence a few helper functions for size calculations that
// multiply integers together, making sure that they're non-negative
// and no overflow occurs.

// return 1 if the sum is valid, 0 on overflow.
// negative terms are considered invalid.
static int stbi__addsizes_valid(int a, int b)
{
	if (b < 0)
		return 0;
	// now 0 <= b <= INT_MAX, hence also
	// 0 <= INT_MAX - b <= INTMAX.
	// And "a + b <= INT_MAX" (which might overflow) is the
	// same as a <= INT_MAX - b (no overflow)
	return a <= INT_MAX - b;
}

// returns 1 if the product is valid, 0 on overflow.
// negative factors are considered invalid.
static int stbi__mul2sizes_valid(int a, int b)
{
	if (a < 0 || b < 0)
		return 0;
	if (b == 0)
		return 1; // mul-by-0 is always safe
	// portable way to check for no overflows in a*b
	return a <= INT_MAX / b;
}

#if !defined(STBI_NO_JPEG) || !defined(STBI_NO_PNG)
// returns 1 if "a*b + add" has no negative terms/factors and doesn't overflow
static int stbi__mad2sizes_valid(int a, int b, int add)
{
	return stbi__mul2sizes_valid(a, b) && stbi__addsizes_valid(a * b, add);
}
#endif

// returns 1 if "a*b*c + add" has no negative terms/factors and doesn't overflow
static int stbi__mad3sizes_valid(int a, int b, int c, int add)
{
	return stbi__mul2sizes_valid(a, b) && stbi__mul2sizes_valid(a * b, c) && stbi__addsizes_valid(a * b * c, add);
}

#if !defined(STBI_NO_JPEG) || !defined(STBI_NO_PNG)
// mallocs with size overflow checking
static void *stbi__malloc_mad2(int a, int b, int add)
{
	if (!stbi__mad2sizes_valid(a, b, add))
		return NULL;
	return stbi__malloc(a * b + add);
}
#endif

static void *stbi__malloc_mad3(int a, int b, int c, int add)
{
	if (!stbi__mad3sizes_valid(a, b, c, add))
		return NULL;
	return stbi__malloc(a * b * c + add);
}

// stbi__err - error
// stbi__errpf - error returning pointer to float
// stbi__errpuc - error returning pointer to unsigned char

#ifdef STBI_NO_FAILURE_STRINGS
	#define stbi__err(x, y) 0
#elif defined(STBI_FAILURE_USERMSG)
	#define stbi__err(x, y) stbi__err(y)
#else
	#define stbi__err(x, y) stbi__err(x)
#endif

#define stbi__errpf(x, y)  ((float *) (size_t)(stbi__err(x, y) ? NULL : NULL))
#define stbi__errpuc(x, y) ((unsigned char *) (size_t)(stbi__err(x, y) ? NULL : NULL))

static int stbi__vertically_flip_on_load_global = 0;

#ifndef STBI_THREAD_LOCAL
	#define stbi__vertically_flip_on_load stbi__vertically_flip_on_load_global
#else
static STBI_THREAD_LOCAL int stbi__vertically_flip_on_load_local, stbi__vertically_flip_on_load_set;

	#define stbi__vertically_flip_on_load \
		(stbi__vertically_flip_on_load_set ? stbi__vertically_flip_on_load_local : stbi__vertically_flip_on_load_global)
#endif // STBI_THREAD_LOCAL

static void *stbi__load_main(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri, int bpc)
{
	memset(ri, 0, sizeof(*ri));         // make sure it's initialized if we add new fields
	ri->bits_per_channel = 8;           // default is 8 so most paths don't have to be changed
	ri->channel_order = STBI_ORDER_RGB; // all current input & output are this, but this is here so we can add BGR order
	ri->num_channels = 0;

#ifndef STBI_NO_JPEG
	if (stbi__jpeg_test(s))
		return stbi__jpeg_load(s, x, y, comp, req_comp, ri);
#endif
#ifndef STBI_NO_PNG
	if (stbi__png_test(s))
		return stbi__png_load(s, x, y, comp, req_comp, ri);
#endif
#ifndef STBI_NO_GIF
	if (stbi__gif_test(s))
		return stbi__gif_load(s, x, y, comp, req_comp, ri);
#endif
	STBI_NOTUSED(bpc);

	return stbi__errpuc("unknown image type", "Image not of any known type, or corrupt");
}

static stbi_uc *stbi__convert_16_to_8(stbi__uint16 *orig, int w, int h, int channels)
{
	int i;
	int img_len = w * h * channels;
	stbi_uc *reduced;

	reduced = (stbi_uc *) stbi__malloc(img_len);
	if (reduced == NULL)
		return stbi__errpuc("outofmem", "Out of memory");

	for (i = 0; i < img_len; ++i)
		reduced[i] = (stbi_uc)((orig[i] >> 8) & 0xFF); // top half of each byte is sufficient approx of 16->8 bit scaling

	STBI_FREE(orig);
	return reduced;
}

static void stbi__vertical_flip(void *image, int w, int h, int bytes_per_pixel)
{
	int row;
	size_t bytes_per_row = (size_t) w * bytes_per_pixel;
	stbi_uc temp[2048];
	stbi_uc *bytes = (stbi_uc *) image;

	for (row = 0; row < (h >> 1); row++) {
		stbi_uc *row0 = bytes + row * bytes_per_row;
		stbi_uc *row1 = bytes + (h - row - 1) * bytes_per_row;
		// swap row0 with row1
		size_t bytes_left = bytes_per_row;
		while (bytes_left) {
			size_t bytes_copy = (bytes_left < sizeof(temp)) ? bytes_left : sizeof(temp);
			memcpy(temp, row0, bytes_copy);
			memcpy(row0, row1, bytes_copy);
			memcpy(row1, temp, bytes_copy);
			row0 += bytes_copy;
			row1 += bytes_copy;
			bytes_left -= bytes_copy;
		}
	}
}

static unsigned char *stbi__load_and_postprocess_8bit(stbi__context *s, int *x, int *y, int *comp, int req_comp)
{
	stbi__result_info ri;
	void *result = stbi__load_main(s, x, y, comp, req_comp, &ri, 8);

	if (result == NULL)
		return NULL;

	// it is the responsibility of the loaders to make sure we get either 8 or 16 bit.
	STBI_ASSERT(ri.bits_per_channel == 8 || ri.bits_per_channel == 16);

	if (ri.bits_per_channel != 8) {
		result = stbi__convert_16_to_8((stbi__uint16 *) result, *x, *y, req_comp == 0 ? *comp : req_comp);
		ri.bits_per_channel = 8;
	}

	// @TODO: move stbi__convert_format to here

	if (stbi__vertically_flip_on_load) {
		int channels = req_comp ? req_comp : *comp;
		stbi__vertical_flip(result, *x, *y, channels * sizeof(stbi_uc));
	}

	return (unsigned char *) result;
}

static stbi_uc *stbi_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp)
{
	stbi__context s;
	stbi__start_mem(&s, buffer, len);
	return stbi__load_and_postprocess_8bit(&s, x, y, comp, req_comp);
}


//////////////////////////////////////////////////////////////////////////////
//
// Common code used by all image loaders
//

enum { STBI__SCAN_load = 0, STBI__SCAN_type, STBI__SCAN_header };

static void stbi__refill_buffer(stbi__context *s)
{
	int n = (s->io.read)(s->io_user_data, (char *) s->buffer_start, s->buflen);
	s->callback_already_read += (int) (s->img_buffer - s->img_buffer_original);
	if (n == 0) {
		// at end of file, treat same as if from memory, but need to handle case
		// where s->img_buffer isn't pointing to safe memory, e.g. 0-byte file
		s->read_from_callbacks = 0;
		s->img_buffer = s->buffer_start;
		s->img_buffer_end = s->buffer_start + 1;
		*s->img_buffer = 0;
	} else {
		s->img_buffer = s->buffer_start;
		s->img_buffer_end = s->buffer_start + n;
	}
}

static inline stbi_uc stbi__get8(stbi__context *s)
{
	if (s->img_buffer < s->img_buffer_end)
		return *s->img_buffer++;
	if (s->read_from_callbacks) {
		stbi__refill_buffer(s);
		return *s->img_buffer++;
	}
	return 0;
}

#if defined(STBI_NO_JPEG)
// nothing
#else
static inline int stbi__at_eof(stbi__context *s)
{
	if (s->io.read) {
		if (!(s->io.eof)(s->io_user_data))
			return 0;
		// if feof() is true, check if buffer = end
		// special case: we've only got the special 0 character at the end
		if (s->read_from_callbacks == 0)
			return 1;
	}

	return s->img_buffer >= s->img_buffer_end;
}
#endif

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG) && defined(STBI_NO_GIF)
// nothing
#else
static void stbi__skip(stbi__context *s, int n)
{
	if (n == 0)
		return; // already there!
	if (n < 0) {
		s->img_buffer = s->img_buffer_end;
		return;
	}
	if (s->io.read) {
		int blen = (int) (s->img_buffer_end - s->img_buffer);
		if (blen < n) {
			s->img_buffer = s->img_buffer_end;
			(s->io.skip)(s->io_user_data, n - blen);
			return;
		}
	}
	s->img_buffer += n;
}
#endif

#if defined(STBI_NO_PNG)
// nothing
#else
static int stbi__getn(stbi__context *s, stbi_uc *buffer, int n)
{
	if (s->io.read) {
		int blen = (int) (s->img_buffer_end - s->img_buffer);
		if (blen < n) {
			int res, count;

			memcpy(buffer, s->img_buffer, blen);

			count = (s->io.read)(s->io_user_data, (char *) buffer + blen, n - blen);
			res = (count == (n - blen));
			s->img_buffer = s->img_buffer_end;
			return res;
		}
	}

	if (s->img_buffer + n <= s->img_buffer_end) {
		memcpy(buffer, s->img_buffer, n);
		s->img_buffer += n;
		return 1;
	} else
		return 0;
}
#endif

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG)
// nothing
#else
static int stbi__get16be(stbi__context *s)
{
	int z = stbi__get8(s);
	return (z << 8) + stbi__get8(s);
}
#endif

#if defined(STBI_NO_PNG)
// nothing
#else
static stbi__uint32 stbi__get32be(stbi__context *s)
{
	stbi__uint32 z = stbi__get16be(s);
	return (z << 16) + stbi__get16be(s);
}
#endif

#if defined(STBI_NO_GIF)
// nothing
#else
static int stbi__get16le(stbi__context *s)
{
	int z = stbi__get8(s);
	return z + (stbi__get8(s) << 8);
}
#endif

#define STBI__BYTECAST(x) ((stbi_uc)((x) &255)) // truncate int to byte without warnings

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG) && defined(STBI_NO_GIF)
// nothing
#else
//////////////////////////////////////////////////////////////////////////////
//
//  generic converter from built-in img_n to req_comp
//    individual types do this automatically as much as possible (e.g. jpeg
//    does all cases internally since it needs to colorspace convert anyway,
//    and it never has alpha, so very few cases ). png can automatically
//    interleave an alpha=255 channel, but falls back to this for other cases
//
//  assume data buffer is malloced, so malloc a new one and free that one
//  only failure mode is malloc failing

static stbi_uc stbi__compute_y(int r, int g, int b)
{
	return (stbi_uc)(((r * 77) + (g * 150) + (29 * b)) >> 8);
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_GIF)
// nothing
#else
static unsigned char *stbi__convert_format(unsigned char *data, int img_n, int req_comp, unsigned int x, unsigned int y)
{
	int i, j;
	unsigned char *good;

	if (req_comp == img_n)
		return data;
	STBI_ASSERT(req_comp >= 1 && req_comp <= 4);

	good = (unsigned char *) stbi__malloc_mad3(req_comp, x, y, 0);
	if (good == NULL) {
		STBI_FREE(data);
		return stbi__errpuc("outofmem", "Out of memory");
	}

	for (j = 0; j < (int) y; ++j) {
		unsigned char *src = data + j * x * img_n;
		unsigned char *dest = good + j * x * req_comp;

	#define STBI__COMBO(a, b) ((a) *8 + (b))
	#define STBI__CASE(a, b) \
		case STBI__COMBO(a, b): \
			for (i = x - 1; i >= 0; --i, src += a, dest += b)
		// convert source image with img_n components to one with req_comp components;
		// avoid switch per pixel, so use switch per scanline and massive macros
		switch (STBI__COMBO(img_n, req_comp)) {
			STBI__CASE(1, 2)
			{
				dest[0] = src[0];
				dest[1] = 255;
			}
			break;
			STBI__CASE(1, 3)
			{
				dest[0] = dest[1] = dest[2] = src[0];
			}
			break;
			STBI__CASE(1, 4)
			{
				dest[0] = dest[1] = dest[2] = src[0];
				dest[3] = 255;
			}
			break;
			STBI__CASE(2, 1)
			{
				dest[0] = src[0];
			}
			break;
			STBI__CASE(2, 3)
			{
				dest[0] = dest[1] = dest[2] = src[0];
			}
			break;
			STBI__CASE(2, 4)
			{
				dest[0] = dest[1] = dest[2] = src[0];
				dest[3] = src[1];
			}
			break;
			STBI__CASE(3, 4)
			{
				dest[0] = src[0];
				dest[1] = src[1];
				dest[2] = src[2];
				dest[3] = 255;
			}
			break;
			STBI__CASE(3, 1)
			{
				dest[0] = stbi__compute_y(src[0], src[1], src[2]);
			}
			break;
			STBI__CASE(3, 2)
			{
				dest[0] = stbi__compute_y(src[0], src[1], src[2]);
				dest[1] = 255;
			}
			break;
			STBI__CASE(4, 1)
			{
				dest[0] = stbi__compute_y(src[0], src[1], src[2]);
			}
			break;
			STBI__CASE(4, 2)
			{
				dest[0] = stbi__compute_y(src[0], src[1], src[2]);
				dest[1] = src[3];
			}
			break;
			STBI__CASE(4, 3)
			{
				dest[0] = src[0];
				dest[1] = src[1];
				dest[2] = src[2];
			}
			break;
			default:
				STBI_ASSERT(0);
				STBI_FREE(data);
				STBI_FREE(good);
				return stbi__errpuc("unsupported", "Unsupported format conversion");
		}
	#undef STBI__CASE
	}

	STBI_FREE(data);
	return good;
}
#endif

#if defined(STBI_NO_PNG)
// nothing
#else
static stbi__uint16 stbi__compute_y_16(int r, int g, int b)
{
	return (stbi__uint16)(((r * 77) + (g * 150) + (29 * b)) >> 8);
}
#endif

#if defined(STBI_NO_PNG)
// nothing
#else
static stbi__uint16 *stbi__convert_format16(stbi__uint16 *data, int img_n, int req_comp, unsigned int x, unsigned int y)
{
	int i, j;
	stbi__uint16 *good;

	if (req_comp == img_n)
		return data;
	STBI_ASSERT(req_comp >= 1 && req_comp <= 4);

	good = (stbi__uint16 *) stbi__malloc(req_comp * x * y * 2);
	if (good == NULL) {
		STBI_FREE(data);
		return (stbi__uint16 *) stbi__errpuc("outofmem", "Out of memory");
	}

	for (j = 0; j < (int) y; ++j) {
		stbi__uint16 *src = data + j * x * img_n;
		stbi__uint16 *dest = good + j * x * req_comp;

	#define STBI__COMBO(a, b) ((a) *8 + (b))
	#define STBI__CASE(a, b) \
		case STBI__COMBO(a, b): \
			for (i = x - 1; i >= 0; --i, src += a, dest += b)
		// convert source image with img_n components to one with req_comp components;
		// avoid switch per pixel, so use switch per scanline and massive macros
		switch (STBI__COMBO(img_n, req_comp)) {
			STBI__CASE(1, 2)
			{
				dest[0] = src[0];
				dest[1] = 0xffff;
			}
			break;
			STBI__CASE(1, 3)
			{
				dest[0] = dest[1] = dest[2] = src[0];
			}
			break;
			STBI__CASE(1, 4)
			{
				dest[0] = dest[1] = dest[2] = src[0];
				dest[3] = 0xffff;
			}
			break;
			STBI__CASE(2, 1)
			{
				dest[0] = src[0];
			}
			break;
			STBI__CASE(2, 3)
			{
				dest[0] = dest[1] = dest[2] = src[0];
			}
			break;
			STBI__CASE(2, 4)
			{
				dest[0] = dest[1] = dest[2] = src[0];
				dest[3] = src[1];
			}
			break;
			STBI__CASE(3, 4)
			{
				dest[0] = src[0];
				dest[1] = src[1];
				dest[2] = src[2];
				dest[3] = 0xffff;
			}
			break;
			STBI__CASE(3, 1)
			{
				dest[0] = stbi__compute_y_16(src[0], src[1], src[2]);
			}
			break;
			STBI__CASE(3, 2)
			{
				dest[0] = stbi__compute_y_16(src[0], src[1], src[2]);
				dest[1] = 0xffff;
			}
			break;
			STBI__CASE(4, 1)
			{
				dest[0] = stbi__compute_y_16(src[0], src[1], src[2]);
			}
			break;
			STBI__CASE(4, 2)
			{
				dest[0] = stbi__compute_y_16(src[0], src[1], src[2]);
				dest[1] = src[3];
			}
			break;
			STBI__CASE(4, 3)
			{
				dest[0] = src[0];
				dest[1] = src[1];
				dest[2] = src[2];
			}
			break;
			default:
				STBI_ASSERT(0);
				STBI_FREE(data);
				STBI_FREE(good);
				return (stbi__uint16 *) stbi__errpuc("unsupported", "Unsupported format conversion");
		}
	#undef STBI__CASE
	}

	STBI_FREE(data);
	return good;
}
#endif


//////////////////////////////////////////////////////////////////////////////
//
//  "baseline" JPEG/JFIF decoder
//
//    simple implementation
//      - doesn't support delayed output of y-dimension
//      - simple interface (only one output format: 8-bit interleaved RGB)
//      - doesn't try to recover corrupt jpegs
//      - doesn't allow partial loading, loading multiple at once
//      - still fast on x86 (copying globals into locals doesn't help x86)
//      - allocates lots of intermediate memory (full size of all components)
//        - non-interleaved case requires this anyway
//        - allows good upsampling (see next)
//    high-quality
//      - upsampled channels are bilinearly interpolated, even across blocks
//      - quality integer IDCT derived from IJG's 'slow'
//    performance
//      - fast huffman; reasonable integer IDCT
//      - some SIMD kernels for common paths on targets with SSE2/NEON
//      - uses a lot of intermediate memory, could cache poorly

#ifndef STBI_NO_JPEG

	// huffman decoding acceleration
	#define FAST_BITS 9 // larger handles more cases; smaller stomps less cache

typedef struct {
	stbi_uc fast[1 << FAST_BITS];
	// weirdly, repacking this into AoS is a 10% speed loss, instead of a win
	stbi__uint16 code[256];
	stbi_uc values[256];
	stbi_uc size[257];
	unsigned int maxcode[18];
	int delta[17]; // old 'firstsymbol' - old 'firstcode'
} stbi__huffman;

typedef struct {
	stbi__context *s;
	stbi__huffman huff_dc[4];
	stbi__huffman huff_ac[4];
	stbi__uint16 dequant[4][64];
	stbi__int16 fast_ac[4][1 << FAST_BITS];

	// sizes for components, interleaved MCUs
	int img_h_max, img_v_max;
	int img_mcu_x, img_mcu_y;
	int img_mcu_w, img_mcu_h;

	// definition of jpeg image component
	struct {
		int id;
		int h, v;
		int tq;
		int hd, ha;
		int dc_pred;

		int x, y, w2, h2;
		stbi_uc *data;
		void *raw_data, *raw_coeff;
		stbi_uc *linebuf;
		short *coeff;         // progressive only
		int coeff_w, coeff_h; // number of 8x8 coefficient blocks
	} img_comp[4];

	stbi__uint32 code_buffer; // jpeg entropy-coded buffer
	int code_bits;            // number of valid bits
	unsigned char marker;     // marker seen while filling entropy buffer
	int nomore;               // flag if we saw a marker so must stop

	int progressive;
	int spec_start;
	int spec_end;
	int succ_high;
	int succ_low;
	int eob_run;
	int jfif;
	int app14_color_transform; // Adobe APP14 tag
	int rgb;

	int scan_n, order[4];
	int restart_interval, todo;

	// kernels
	void (*idct_block_kernel)(stbi_uc *out, int out_stride, short data[64]);
	void (*YCbCr_to_RGB_kernel)(stbi_uc *out, const stbi_uc *y, const stbi_uc *pcb, const stbi_uc *pcr, int count, int step);
	stbi_uc *(*resample_row_hv_2_kernel)(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs);
} stbi__jpeg;

static int stbi__build_huffman(stbi__huffman *h, int *count)
{
	int i, j, k = 0;
	unsigned int code;
	// build size list for each symbol (from JPEG spec)
	for (i = 0; i < 16; ++i)
		for (j = 0; j < count[i]; ++j)
			h->size[k++] = (stbi_uc)(i + 1);
	h->size[k] = 0;

	// compute actual symbols (from jpeg spec)
	code = 0;
	k = 0;
	for (j = 1; j <= 16; ++j) {
		// compute delta to add to code to compute symbol id
		h->delta[j] = k - code;
		if (h->size[k] == j) {
			while (h->size[k] == j)
				h->code[k++] = (stbi__uint16)(code++);
			if (code - 1 >= (1u << j))
				return stbi__err("bad code lengths", "Corrupt JPEG");
		}
		// compute largest code + 1 for this size, preshifted as needed later
		h->maxcode[j] = code << (16 - j);
		code <<= 1;
	}
	h->maxcode[j] = 0xffffffff;

	// build non-spec acceleration table; 255 is flag for not-accelerated
	memset(h->fast, 255, 1 << FAST_BITS);
	for (i = 0; i < k; ++i) {
		int s = h->size[i];
		if (s <= FAST_BITS) {
			int c = h->code[i] << (FAST_BITS - s);
			int m = 1 << (FAST_BITS - s);
			for (j = 0; j < m; ++j) {
				h->fast[c + j] = (stbi_uc) i;
			}
		}
	}
	return 1;
}

// build a table that decodes both magnitude and value of small ACs in
// one go.
static void stbi__build_fast_ac(stbi__int16 *fast_ac, stbi__huffman *h)
{
	int i;
	for (i = 0; i < (1 << FAST_BITS); ++i) {
		stbi_uc fast = h->fast[i];
		fast_ac[i] = 0;
		if (fast < 255) {
			int rs = h->values[fast];
			int run = (rs >> 4) & 15;
			int magbits = rs & 15;
			int len = h->size[fast];

			if (magbits && len + magbits <= FAST_BITS) {
				// magnitude code followed by receive_extend code
				int k = ((i << len) & ((1 << FAST_BITS) - 1)) >> (FAST_BITS - magbits);
				int m = 1 << (magbits - 1);
				if (k < m)
					k += (~0U << magbits) + 1;
				// if the result is small enough, we can fit it in fast_ac table
				if (k >= -128 && k <= 127)
					fast_ac[i] = (stbi__int16)((k * 256) + (run * 16) + (len + magbits));
			}
		}
	}
}

static void stbi__grow_buffer_unsafe(stbi__jpeg *j)
{
	do {
		unsigned int b = j->nomore ? 0 : stbi__get8(j->s);
		if (b == 0xff) {
			int c = stbi__get8(j->s);
			while (c == 0xff)
				c = stbi__get8(j->s); // consume fill bytes
			if (c != 0) {
				j->marker = (unsigned char) c;
				j->nomore = 1;
				return;
			}
		}
		j->code_buffer |= b << (24 - j->code_bits);
		j->code_bits += 8;
	} while (j->code_bits <= 24);
}

// (1 << n) - 1
static const stbi__uint32 stbi__bmask[17] = {0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535};

// decode a jpeg huffman value from the bitstream
static inline int stbi__jpeg_huff_decode(stbi__jpeg *j, stbi__huffman *h)
{
	unsigned int temp;
	int c, k;

	if (j->code_bits < 16)
		stbi__grow_buffer_unsafe(j);

	// look at the top FAST_BITS and determine what symbol ID it is,
	// if the code is <= FAST_BITS
	c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS) - 1);
	k = h->fast[c];
	if (k < 255) {
		int s = h->size[k];
		if (s > j->code_bits)
			return -1;
		j->code_buffer <<= s;
		j->code_bits -= s;
		return h->values[k];
	}

	// naive test is to shift the code_buffer down so k bits are
	// valid, then test against maxcode. To speed this up, we've
	// preshifted maxcode left so that it has (16-k) 0s at the
	// end; in other words, regardless of the number of bits, it
	// wants to be compared against something shifted to have 16;
	// that way we don't need to shift inside the loop.
	temp = j->code_buffer >> 16;
	for (k = FAST_BITS + 1;; ++k)
		if (temp < h->maxcode[k])
			break;
	if (k == 17) {
		// error! code not found
		j->code_bits -= 16;
		return -1;
	}

	if (k > j->code_bits)
		return -1;

	// convert the huffman code to the symbol id
	c = ((j->code_buffer >> (32 - k)) & stbi__bmask[k]) + h->delta[k];
	STBI_ASSERT((((j->code_buffer) >> (32 - h->size[c])) & stbi__bmask[h->size[c]]) == h->code[c]);

	// convert the id to a symbol
	j->code_bits -= k;
	j->code_buffer <<= k;
	return h->values[c];
}

// bias[n] = (-1<<n) + 1
static const int stbi__jbias[16] = {0, -1, -3, -7, -15, -31, -63, -127, -255, -511, -1023, -2047, -4095, -8191, -16383, -32767};

// combined JPEG 'receive' and JPEG 'extend', since baseline
// always extends everything it receives.
static inline int stbi__extend_receive(stbi__jpeg *j, int n)
{
	unsigned int k;
	int sgn;
	if (j->code_bits < n)
		stbi__grow_buffer_unsafe(j);

	sgn = (stbi__int32) j->code_buffer >> 31; // sign bit is always in MSB
	k = stbi_lrot(j->code_buffer, n);
	if (n < 0 || n >= (int) (sizeof(stbi__bmask) / sizeof(*stbi__bmask)))
		return 0;
	j->code_buffer = k & ~stbi__bmask[n];
	k &= stbi__bmask[n];
	j->code_bits -= n;
	return k + (stbi__jbias[n] & ~sgn);
}

// get some unsigned bits
static inline int stbi__jpeg_get_bits(stbi__jpeg *j, int n)
{
	unsigned int k;
	if (j->code_bits < n)
		stbi__grow_buffer_unsafe(j);
	k = stbi_lrot(j->code_buffer, n);
	j->code_buffer = k & ~stbi__bmask[n];
	k &= stbi__bmask[n];
	j->code_bits -= n;
	return k;
}

static inline int stbi__jpeg_get_bit(stbi__jpeg *j)
{
	unsigned int k;
	if (j->code_bits < 1)
		stbi__grow_buffer_unsafe(j);
	k = j->code_buffer;
	j->code_buffer <<= 1;
	--j->code_bits;
	return k & 0x80000000;
}

// given a value that's at position X in the zigzag stream,
// where does it appear in the 8x8 matrix coded as row-major?
static const stbi_uc stbi__jpeg_dezigzag[64 + 15] = {0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
	// let corrupt input sample past end
	63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63};

// decode one 64-entry block--
static int stbi__jpeg_decode_block(stbi__jpeg *j, short data[64], stbi__huffman *hdc, stbi__huffman *hac, stbi__int16 *fac, int b, stbi__uint16 *dequant)
{
	int diff, dc, k;
	int t;

	if (j->code_bits < 16)
		stbi__grow_buffer_unsafe(j);
	t = stbi__jpeg_huff_decode(j, hdc);
	if (t < 0)
		return stbi__err("bad huffman code", "Corrupt JPEG");

	// 0 all the ac values now so we can do it 32-bits at a time
	memset(data, 0, 64 * sizeof(data[0]));

	diff = t ? stbi__extend_receive(j, t) : 0;
	dc = j->img_comp[b].dc_pred + diff;
	j->img_comp[b].dc_pred = dc;
	data[0] = (short) (dc * dequant[0]);

	// decode AC components, see JPEG spec
	k = 1;
	do {
		unsigned int zig;
		int c, r, s;
		if (j->code_bits < 16)
			stbi__grow_buffer_unsafe(j);
		c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS) - 1);
		r = fac[c];
		if (r) {                // fast-AC path
			k += (r >> 4) & 15; // run
			s = r & 15;         // combined length
			j->code_buffer <<= s;
			j->code_bits -= s;
			// decode into unzigzag'd location
			zig = stbi__jpeg_dezigzag[k++];
			data[zig] = (short) ((r >> 8) * dequant[zig]);
		} else {
			int rs = stbi__jpeg_huff_decode(j, hac);
			if (rs < 0)
				return stbi__err("bad huffman code", "Corrupt JPEG");
			s = rs & 15;
			r = rs >> 4;
			if (s == 0) {
				if (rs != 0xf0)
					break; // end block
				k += 16;
			} else {
				k += r;
				// decode into unzigzag'd location
				zig = stbi__jpeg_dezigzag[k++];
				data[zig] = (short) (stbi__extend_receive(j, s) * dequant[zig]);
			}
		}
	} while (k < 64);
	return 1;
}

static int stbi__jpeg_decode_block_prog_dc(stbi__jpeg *j, short data[64], stbi__huffman *hdc, int b)
{
	int diff, dc;
	int t;
	if (j->spec_end != 0)
		return stbi__err("can't merge dc and ac", "Corrupt JPEG");

	if (j->code_bits < 16)
		stbi__grow_buffer_unsafe(j);

	if (j->succ_high == 0) {
		// first scan for DC coefficient, must be first
		memset(data, 0, 64 * sizeof(data[0])); // 0 all the ac values now
		t = stbi__jpeg_huff_decode(j, hdc);
		if (t == -1)
			return stbi__err("can't merge dc and ac", "Corrupt JPEG");
		diff = t ? stbi__extend_receive(j, t) : 0;

		dc = j->img_comp[b].dc_pred + diff;
		j->img_comp[b].dc_pred = dc;
		data[0] = (short) (dc << j->succ_low);
	} else {
		// refinement scan for DC coefficient
		if (stbi__jpeg_get_bit(j))
			data[0] += (short) (1 << j->succ_low);
	}
	return 1;
}

// @OPTIMIZE: store non-zigzagged during the decode passes,
// and only de-zigzag when dequantizing
static int stbi__jpeg_decode_block_prog_ac(stbi__jpeg *j, short data[64], stbi__huffman *hac, stbi__int16 *fac)
{
	int k;
	if (j->spec_start == 0)
		return stbi__err("can't merge dc and ac", "Corrupt JPEG");

	if (j->succ_high == 0) {
		int shift = j->succ_low;

		if (j->eob_run) {
			--j->eob_run;
			return 1;
		}

		k = j->spec_start;
		do {
			unsigned int zig;
			int c, r, s;
			if (j->code_bits < 16)
				stbi__grow_buffer_unsafe(j);
			c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS) - 1);
			r = fac[c];
			if (r) {                // fast-AC path
				k += (r >> 4) & 15; // run
				s = r & 15;         // combined length
				j->code_buffer <<= s;
				j->code_bits -= s;
				zig = stbi__jpeg_dezigzag[k++];
				data[zig] = (short) ((r >> 8) << shift);
			} else {
				int rs = stbi__jpeg_huff_decode(j, hac);
				if (rs < 0)
					return stbi__err("bad huffman code", "Corrupt JPEG");
				s = rs & 15;
				r = rs >> 4;
				if (s == 0) {
					if (r < 15) {
						j->eob_run = (1 << r);
						if (r)
							j->eob_run += stbi__jpeg_get_bits(j, r);
						--j->eob_run;
						break;
					}
					k += 16;
				} else {
					k += r;
					zig = stbi__jpeg_dezigzag[k++];
					data[zig] = (short) (stbi__extend_receive(j, s) << shift);
				}
			}
		} while (k <= j->spec_end);
	} else {
		// refinement scan for these AC coefficients

		short bit = (short) (1 << j->succ_low);

		if (j->eob_run) {
			--j->eob_run;
			for (k = j->spec_start; k <= j->spec_end; ++k) {
				short *p = &data[stbi__jpeg_dezigzag[k]];
				if (*p != 0)
					if (stbi__jpeg_get_bit(j))
						if ((*p & bit) == 0) {
							if (*p > 0)
								*p += bit;
							else
								*p -= bit;
						}
			}
		} else {
			k = j->spec_start;
			do {
				int r, s;
				int rs = stbi__jpeg_huff_decode(j, hac); // @OPTIMIZE see if we can use the fast path here, advance-by-r is so slow, eh
				if (rs < 0)
					return stbi__err("bad huffman code", "Corrupt JPEG");
				s = rs & 15;
				r = rs >> 4;
				if (s == 0) {
					if (r < 15) {
						j->eob_run = (1 << r) - 1;
						if (r)
							j->eob_run += stbi__jpeg_get_bits(j, r);
						r = 64; // force end of block
					} else {
						// r=15 s=0 should write 16 0s, so we just do
						// a run of 15 0s and then write s (which is 0),
						// so we don't have to do anything special here
					}
				} else {
					if (s != 1)
						return stbi__err("bad huffman code", "Corrupt JPEG");
					// sign bit
					if (stbi__jpeg_get_bit(j))
						s = bit;
					else
						s = -bit;
				}

				// advance by r
				while (k <= j->spec_end) {
					short *p = &data[stbi__jpeg_dezigzag[k++]];
					if (*p != 0) {
						if (stbi__jpeg_get_bit(j))
							if ((*p & bit) == 0) {
								if (*p > 0)
									*p += bit;
								else
									*p -= bit;
							}
					} else {
						if (r == 0) {
							*p = (short) s;
							break;
						}
						--r;
					}
				}
			} while (k <= j->spec_end);
		}
	}
	return 1;
}

// take a -128..127 value and stbi__clamp it and convert to 0..255
static inline stbi_uc stbi__clamp(int x)
{
	// trick to use a single test to catch both cases
	if ((unsigned int) x > 255) {
		if (x < 0)
			return 0;
		if (x > 255)
			return 255;
	}
	return (stbi_uc) x;
}

	#define stbi__f2f(x) ((int) (((x) *4096 + 0.5)))
	#define stbi__fsh(x) ((x) *4096)

	// derived from jidctint -- DCT_ISLOW
	#define STBI__IDCT_1D(s0, s1, s2, s3, s4, s5, s6, s7) \
		int t0, t1, t2, t3, p1, p2, p3, p4, p5, x0, x1, x2, x3; \
		p2 = s2; \
		p3 = s6; \
		p1 = (p2 + p3) * stbi__f2f(0.5411961f); \
		t2 = p1 + p3 * stbi__f2f(-1.847759065f); \
		t3 = p1 + p2 * stbi__f2f(0.765366865f); \
		p2 = s0; \
		p3 = s4; \
		t0 = stbi__fsh(p2 + p3); \
		t1 = stbi__fsh(p2 - p3); \
		x0 = t0 + t3; \
		x3 = t0 - t3; \
		x1 = t1 + t2; \
		x2 = t1 - t2; \
		t0 = s7; \
		t1 = s5; \
		t2 = s3; \
		t3 = s1; \
		p3 = t0 + t2; \
		p4 = t1 + t3; \
		p1 = t0 + t3; \
		p2 = t1 + t2; \
		p5 = (p3 + p4) * stbi__f2f(1.175875602f); \
		t0 = t0 * stbi__f2f(0.298631336f); \
		t1 = t1 * stbi__f2f(2.053119869f); \
		t2 = t2 * stbi__f2f(3.072711026f); \
		t3 = t3 * stbi__f2f(1.501321110f); \
		p1 = p5 + p1 * stbi__f2f(-0.899976223f); \
		p2 = p5 + p2 * stbi__f2f(-2.562915447f); \
		p3 = p3 * stbi__f2f(-1.961570560f); \
		p4 = p4 * stbi__f2f(-0.390180644f); \
		t3 += p1 + p4; \
		t2 += p2 + p3; \
		t1 += p2 + p4; \
		t0 += p1 + p3;

static void stbi__idct_block(stbi_uc *out, int out_stride, short data[64])
{
	int i, val[64], *v = val;
	stbi_uc *o;
	short *d = data;

	// columns
	for (i = 0; i < 8; ++i, ++d, ++v) {
		// if all zeroes, shortcut -- this avoids dequantizing 0s and IDCTing
		if (d[8] == 0 && d[16] == 0 && d[24] == 0 && d[32] == 0 && d[40] == 0 && d[48] == 0 && d[56] == 0) {
			//    no shortcut                 0     seconds
			//    (1|2|3|4|5|6|7)==0          0     seconds
			//    all separate               -0.047 seconds
			//    1 && 2|3 && 4|5 && 6|7:    -0.047 seconds
			int dcterm = d[0] * 4;
			v[0] = v[8] = v[16] = v[24] = v[32] = v[40] = v[48] = v[56] = dcterm;
		} else {
			STBI__IDCT_1D(d[0], d[8], d[16], d[24], d[32], d[40], d[48], d[56])
			// constants scaled things up by 1<<12; let's bring them back
			// down, but keep 2 extra bits of precision
			x0 += 512;
			x1 += 512;
			x2 += 512;
			x3 += 512;
			v[0] = (x0 + t3) >> 10;
			v[56] = (x0 - t3) >> 10;
			v[8] = (x1 + t2) >> 10;
			v[48] = (x1 - t2) >> 10;
			v[16] = (x2 + t1) >> 10;
			v[40] = (x2 - t1) >> 10;
			v[24] = (x3 + t0) >> 10;
			v[32] = (x3 - t0) >> 10;
		}
	}

	for (i = 0, v = val, o = out; i < 8; ++i, v += 8, o += out_stride) {
		// no fast case since the first 1D IDCT spread components out
		STBI__IDCT_1D(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7])
		// constants scaled things up by 1<<12, plus we had 1<<2 from first
		// loop, plus horizontal and vertical each scale by sqrt(8) so together
		// we've got an extra 1<<3, so 1<<17 total we need to remove.
		// so we want to round that, which means adding 0.5 * 1<<17,
		// aka 65536. Also, we'll end up with -128 to 127 that we want
		// to encode as 0..255 by adding 128, so we'll add that before the shift
		x0 += 65536 + (128 << 17);
		x1 += 65536 + (128 << 17);
		x2 += 65536 + (128 << 17);
		x3 += 65536 + (128 << 17);
		// tried computing the shifts into temps, or'ing the temps to see
		// if any were out of range, but that was slower
		o[0] = stbi__clamp((x0 + t3) >> 17);
		o[7] = stbi__clamp((x0 - t3) >> 17);
		o[1] = stbi__clamp((x1 + t2) >> 17);
		o[6] = stbi__clamp((x1 - t2) >> 17);
		o[2] = stbi__clamp((x2 + t1) >> 17);
		o[5] = stbi__clamp((x2 - t1) >> 17);
		o[3] = stbi__clamp((x3 + t0) >> 17);
		o[4] = stbi__clamp((x3 - t0) >> 17);
	}
}

	#ifdef STBI_SSE2
// sse2 integer IDCT. not the fastest possible implementation but it
// produces bit-identical results to the generic C version so it's
// fully "transparent".
static void stbi__idct_simd(stbi_uc *out, int out_stride, short data[64])
{
	// This is constructed to match our regular (generic) integer IDCT exactly.
	__m128i row0, row1, row2, row3, row4, row5, row6, row7;
	__m128i tmp;

		// dot product constant: even elems=x, odd elems=y
		#define dct_const(x, y) _mm_setr_epi16((x), (y), (x), (y), (x), (y), (x), (y))

		// out(0) = c0[even]*x + c0[odd]*y   (c0, x, y 16-bit, out 32-bit)
		// out(1) = c1[even]*x + c1[odd]*y
		#define dct_rot(out0, out1, x, y, c0, c1) \
			__m128i c0##lo = _mm_unpacklo_epi16((x), (y)); \
			__m128i c0##hi = _mm_unpackhi_epi16((x), (y)); \
			__m128i out0##_l = _mm_madd_epi16(c0##lo, c0); \
			__m128i out0##_h = _mm_madd_epi16(c0##hi, c0); \
			__m128i out1##_l = _mm_madd_epi16(c0##lo, c1); \
			__m128i out1##_h = _mm_madd_epi16(c0##hi, c1)

		// out = in << 12  (in 16-bit, out 32-bit)
		#define dct_widen(out, in) \
			__m128i out##_l = _mm_srai_epi32(_mm_unpacklo_epi16(_mm_setzero_si128(), (in)), 4); \
			__m128i out##_h = _mm_srai_epi32(_mm_unpackhi_epi16(_mm_setzero_si128(), (in)), 4)

		// wide add
		#define dct_wadd(out, a, b) \
			__m128i out##_l = _mm_add_epi32(a##_l, b##_l); \
			__m128i out##_h = _mm_add_epi32(a##_h, b##_h)

		// wide sub
		#define dct_wsub(out, a, b) \
			__m128i out##_l = _mm_sub_epi32(a##_l, b##_l); \
			__m128i out##_h = _mm_sub_epi32(a##_h, b##_h)

		// butterfly a/b, add bias, then shift by "s" and pack
		#define dct_bfly32o(out0, out1, a, b, bias, s) \
			{ \
				__m128i abiased_l = _mm_add_epi32(a##_l, bias); \
				__m128i abiased_h = _mm_add_epi32(a##_h, bias); \
				dct_wadd(sum, abiased, b); \
				dct_wsub(dif, abiased, b); \
				out0 = _mm_packs_epi32(_mm_srai_epi32(sum_l, s), _mm_srai_epi32(sum_h, s)); \
				out1 = _mm_packs_epi32(_mm_srai_epi32(dif_l, s), _mm_srai_epi32(dif_h, s)); \
			}

		// 8-bit interleave step (for transposes)
		#define dct_interleave8(a, b) \
			tmp = a; \
			a = _mm_unpacklo_epi8(a, b); \
			b = _mm_unpackhi_epi8(tmp, b)

		// 16-bit interleave step (for transposes)
		#define dct_interleave16(a, b) \
			tmp = a; \
			a = _mm_unpacklo_epi16(a, b); \
			b = _mm_unpackhi_epi16(tmp, b)

		#define dct_pass(bias, shift) \
			{ \
				/* even part */ \
				dct_rot(t2e, t3e, row2, row6, rot0_0, rot0_1); \
				__m128i sum04 = _mm_add_epi16(row0, row4); \
				__m128i dif04 = _mm_sub_epi16(row0, row4); \
				dct_widen(t0e, sum04); \
				dct_widen(t1e, dif04); \
				dct_wadd(x0, t0e, t3e); \
				dct_wsub(x3, t0e, t3e); \
				dct_wadd(x1, t1e, t2e); \
				dct_wsub(x2, t1e, t2e); \
				/* odd part */ \
				dct_rot(y0o, y2o, row7, row3, rot2_0, rot2_1); \
				dct_rot(y1o, y3o, row5, row1, rot3_0, rot3_1); \
				__m128i sum17 = _mm_add_epi16(row1, row7); \
				__m128i sum35 = _mm_add_epi16(row3, row5); \
				dct_rot(y4o, y5o, sum17, sum35, rot1_0, rot1_1); \
				dct_wadd(x4, y0o, y4o); \
				dct_wadd(x5, y1o, y5o); \
				dct_wadd(x6, y2o, y5o); \
				dct_wadd(x7, y3o, y4o); \
				dct_bfly32o(row0, row7, x0, x7, bias, shift); \
				dct_bfly32o(row1, row6, x1, x6, bias, shift); \
				dct_bfly32o(row2, row5, x2, x5, bias, shift); \
				dct_bfly32o(row3, row4, x3, x4, bias, shift); \
			}

	__m128i rot0_0 = dct_const(stbi__f2f(0.5411961f), stbi__f2f(0.5411961f) + stbi__f2f(-1.847759065f));
	__m128i rot0_1 = dct_const(stbi__f2f(0.5411961f) + stbi__f2f(0.765366865f), stbi__f2f(0.5411961f));
	__m128i rot1_0 = dct_const(stbi__f2f(1.175875602f) + stbi__f2f(-0.899976223f), stbi__f2f(1.175875602f));
	__m128i rot1_1 = dct_const(stbi__f2f(1.175875602f), stbi__f2f(1.175875602f) + stbi__f2f(-2.562915447f));
	__m128i rot2_0 = dct_const(stbi__f2f(-1.961570560f) + stbi__f2f(0.298631336f), stbi__f2f(-1.961570560f));
	__m128i rot2_1 = dct_const(stbi__f2f(-1.961570560f), stbi__f2f(-1.961570560f) + stbi__f2f(3.072711026f));
	__m128i rot3_0 = dct_const(stbi__f2f(-0.390180644f) + stbi__f2f(2.053119869f), stbi__f2f(-0.390180644f));
	__m128i rot3_1 = dct_const(stbi__f2f(-0.390180644f), stbi__f2f(-0.390180644f) + stbi__f2f(1.501321110f));

	// rounding biases in column/row passes, see stbi__idct_block for explanation.
	__m128i bias_0 = _mm_set1_epi32(512);
	__m128i bias_1 = _mm_set1_epi32(65536 + (128 << 17));

	// load
	row0 = _mm_load_si128((const __m128i *) (data + 0 * 8));
	row1 = _mm_load_si128((const __m128i *) (data + 1 * 8));
	row2 = _mm_load_si128((const __m128i *) (data + 2 * 8));
	row3 = _mm_load_si128((const __m128i *) (data + 3 * 8));
	row4 = _mm_load_si128((const __m128i *) (data + 4 * 8));
	row5 = _mm_load_si128((const __m128i *) (data + 5 * 8));
	row6 = _mm_load_si128((const __m128i *) (data + 6 * 8));
	row7 = _mm_load_si128((const __m128i *) (data + 7 * 8));

	// column pass
	dct_pass(bias_0, 10);

	{
		// 16bit 8x8 transpose pass 1
		dct_interleave16(row0, row4);
		dct_interleave16(row1, row5);
		dct_interleave16(row2, row6);
		dct_interleave16(row3, row7);

		// transpose pass 2
		dct_interleave16(row0, row2);
		dct_interleave16(row1, row3);
		dct_interleave16(row4, row6);
		dct_interleave16(row5, row7);

		// transpose pass 3
		dct_interleave16(row0, row1);
		dct_interleave16(row2, row3);
		dct_interleave16(row4, row5);
		dct_interleave16(row6, row7);
	}

	// row pass
	dct_pass(bias_1, 17);

	{
		// pack
		__m128i p0 = _mm_packus_epi16(row0, row1); // a0a1a2a3...a7b0b1b2b3...b7
		__m128i p1 = _mm_packus_epi16(row2, row3);
		__m128i p2 = _mm_packus_epi16(row4, row5);
		__m128i p3 = _mm_packus_epi16(row6, row7);

		// 8bit 8x8 transpose pass 1
		dct_interleave8(p0, p2); // a0e0a1e1...
		dct_interleave8(p1, p3); // c0g0c1g1...

		// transpose pass 2
		dct_interleave8(p0, p1); // a0c0e0g0...
		dct_interleave8(p2, p3); // b0d0f0h0...

		// transpose pass 3
		dct_interleave8(p0, p2); // a0b0c0d0...
		dct_interleave8(p1, p3); // a4b4c4d4...

		// store
		_mm_storel_epi64((__m128i *) out, p0);
		out += out_stride;
		_mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p0, 0x4e));
		out += out_stride;
		_mm_storel_epi64((__m128i *) out, p2);
		out += out_stride;
		_mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p2, 0x4e));
		out += out_stride;
		_mm_storel_epi64((__m128i *) out, p1);
		out += out_stride;
		_mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p1, 0x4e));
		out += out_stride;
		_mm_storel_epi64((__m128i *) out, p3);
		out += out_stride;
		_mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p3, 0x4e));
	}

		#undef dct_const
		#undef dct_rot
		#undef dct_widen
		#undef dct_wadd
		#undef dct_wsub
		#undef dct_bfly32o
		#undef dct_interleave8
		#undef dct_interleave16
		#undef dct_pass
}

	#endif // STBI_SSE2

	#ifdef STBI_NEON

// NEON integer IDCT. should produce bit-identical
// results to the generic C version.
static void stbi__idct_simd(stbi_uc *out, int out_stride, short data[64])
{
	int16x8_t row0, row1, row2, row3, row4, row5, row6, row7;

	int16x4_t rot0_0 = vdup_n_s16(stbi__f2f(0.5411961f));
	int16x4_t rot0_1 = vdup_n_s16(stbi__f2f(-1.847759065f));
	int16x4_t rot0_2 = vdup_n_s16(stbi__f2f(0.765366865f));
	int16x4_t rot1_0 = vdup_n_s16(stbi__f2f(1.175875602f));
	int16x4_t rot1_1 = vdup_n_s16(stbi__f2f(-0.899976223f));
	int16x4_t rot1_2 = vdup_n_s16(stbi__f2f(-2.562915447f));
	int16x4_t rot2_0 = vdup_n_s16(stbi__f2f(-1.961570560f));
	int16x4_t rot2_1 = vdup_n_s16(stbi__f2f(-0.390180644f));
	int16x4_t rot3_0 = vdup_n_s16(stbi__f2f(0.298631336f));
	int16x4_t rot3_1 = vdup_n_s16(stbi__f2f(2.053119869f));
	int16x4_t rot3_2 = vdup_n_s16(stbi__f2f(3.072711026f));
	int16x4_t rot3_3 = vdup_n_s16(stbi__f2f(1.501321110f));

		#define dct_long_mul(out, inq, coeff) \
			int32x4_t out##_l = vmull_s16(vget_low_s16(inq), coeff); \
			int32x4_t out##_h = vmull_s16(vget_high_s16(inq), coeff)

		#define dct_long_mac(out, acc, inq, coeff) \
			int32x4_t out##_l = vmlal_s16(acc##_l, vget_low_s16(inq), coeff); \
			int32x4_t out##_h = vmlal_s16(acc##_h, vget_high_s16(inq), coeff)

		#define dct_widen(out, inq) \
			int32x4_t out##_l = vshll_n_s16(vget_low_s16(inq), 12); \
			int32x4_t out##_h = vshll_n_s16(vget_high_s16(inq), 12)

		// wide add
		#define dct_wadd(out, a, b) \
			int32x4_t out##_l = vaddq_s32(a##_l, b##_l); \
			int32x4_t out##_h = vaddq_s32(a##_h, b##_h)

		// wide sub
		#define dct_wsub(out, a, b) \
			int32x4_t out##_l = vsubq_s32(a##_l, b##_l); \
			int32x4_t out##_h = vsubq_s32(a##_h, b##_h)

		// butterfly a/b, then shift using "shiftop" by "s" and pack
		#define dct_bfly32o(out0, out1, a, b, shiftop, s) \
			{ \
				dct_wadd(sum, a, b); \
				dct_wsub(dif, a, b); \
				out0 = vcombine_s16(shiftop(sum_l, s), shiftop(sum_h, s)); \
				out1 = vcombine_s16(shiftop(dif_l, s), shiftop(dif_h, s)); \
			}

		#define dct_pass(shiftop, shift) \
			{ \
				/* even part */ \
				int16x8_t sum26 = vaddq_s16(row2, row6); \
				dct_long_mul(p1e, sum26, rot0_0); \
				dct_long_mac(t2e, p1e, row6, rot0_1); \
				dct_long_mac(t3e, p1e, row2, rot0_2); \
				int16x8_t sum04 = vaddq_s16(row0, row4); \
				int16x8_t dif04 = vsubq_s16(row0, row4); \
				dct_widen(t0e, sum04); \
				dct_widen(t1e, dif04); \
				dct_wadd(x0, t0e, t3e); \
				dct_wsub(x3, t0e, t3e); \
				dct_wadd(x1, t1e, t2e); \
				dct_wsub(x2, t1e, t2e); \
				/* odd part */ \
				int16x8_t sum15 = vaddq_s16(row1, row5); \
				int16x8_t sum17 = vaddq_s16(row1, row7); \
				int16x8_t sum35 = vaddq_s16(row3, row5); \
				int16x8_t sum37 = vaddq_s16(row3, row7); \
				int16x8_t sumodd = vaddq_s16(sum17, sum35); \
				dct_long_mul(p5o, sumodd, rot1_0); \
				dct_long_mac(p1o, p5o, sum17, rot1_1); \
				dct_long_mac(p2o, p5o, sum35, rot1_2); \
				dct_long_mul(p3o, sum37, rot2_0); \
				dct_long_mul(p4o, sum15, rot2_1); \
				dct_wadd(sump13o, p1o, p3o); \
				dct_wadd(sump24o, p2o, p4o); \
				dct_wadd(sump23o, p2o, p3o); \
				dct_wadd(sump14o, p1o, p4o); \
				dct_long_mac(x4, sump13o, row7, rot3_0); \
				dct_long_mac(x5, sump24o, row5, rot3_1); \
				dct_long_mac(x6, sump23o, row3, rot3_2); \
				dct_long_mac(x7, sump14o, row1, rot3_3); \
				dct_bfly32o(row0, row7, x0, x7, shiftop, shift); \
				dct_bfly32o(row1, row6, x1, x6, shiftop, shift); \
				dct_bfly32o(row2, row5, x2, x5, shiftop, shift); \
				dct_bfly32o(row3, row4, x3, x4, shiftop, shift); \
			}

	// load
	row0 = vld1q_s16(data + 0 * 8);
	row1 = vld1q_s16(data + 1 * 8);
	row2 = vld1q_s16(data + 2 * 8);
	row3 = vld1q_s16(data + 3 * 8);
	row4 = vld1q_s16(data + 4 * 8);
	row5 = vld1q_s16(data + 5 * 8);
	row6 = vld1q_s16(data + 6 * 8);
	row7 = vld1q_s16(data + 7 * 8);

	// add DC bias
	row0 = vaddq_s16(row0, vsetq_lane_s16(1024, vdupq_n_s16(0), 0));

	// column pass
	dct_pass(vrshrn_n_s32, 10);

	// 16bit 8x8 transpose
	{
		// these three map to a single VTRN.16, VTRN.32, and VSWP, respectively.
		// whether compilers actually get this is another story, sadly.
		#define dct_trn16(x, y) \
			{ \
				int16x8x2_t t = vtrnq_s16(x, y); \
				x = t.val[0]; \
				y = t.val[1]; \
			}
		#define dct_trn32(x, y) \
			{ \
				int32x4x2_t t = vtrnq_s32(vreinterpretq_s32_s16(x), vreinterpretq_s32_s16(y)); \
				x = vreinterpretq_s16_s32(t.val[0]); \
				y = vreinterpretq_s16_s32(t.val[1]); \
			}
		#define dct_trn64(x, y) \
			{ \
				int16x8_t x0 = x; \
				int16x8_t y0 = y; \
				x = vcombine_s16(vget_low_s16(x0), vget_low_s16(y0)); \
				y = vcombine_s16(vget_high_s16(x0), vget_high_s16(y0)); \
			}

		// pass 1
		dct_trn16(row0, row1); // a0b0a2b2a4b4a6b6
		dct_trn16(row2, row3);
		dct_trn16(row4, row5);
		dct_trn16(row6, row7);

		// pass 2
		dct_trn32(row0, row2); // a0b0c0d0a4b4c4d4
		dct_trn32(row1, row3);
		dct_trn32(row4, row6);
		dct_trn32(row5, row7);

		// pass 3
		dct_trn64(row0, row4); // a0b0c0d0e0f0g0h0
		dct_trn64(row1, row5);
		dct_trn64(row2, row6);
		dct_trn64(row3, row7);

		#undef dct_trn16
		#undef dct_trn32
		#undef dct_trn64
	}

	// row pass
	// vrshrn_n_s32 only supports shifts up to 16, we need
	// 17. so do a non-rounding shift of 16 first then follow
	// up with a rounding shift by 1.
	dct_pass(vshrn_n_s32, 16);

	{
		// pack and round
		uint8x8_t p0 = vqrshrun_n_s16(row0, 1);
		uint8x8_t p1 = vqrshrun_n_s16(row1, 1);
		uint8x8_t p2 = vqrshrun_n_s16(row2, 1);
		uint8x8_t p3 = vqrshrun_n_s16(row3, 1);
		uint8x8_t p4 = vqrshrun_n_s16(row4, 1);
		uint8x8_t p5 = vqrshrun_n_s16(row5, 1);
		uint8x8_t p6 = vqrshrun_n_s16(row6, 1);
		uint8x8_t p7 = vqrshrun_n_s16(row7, 1);

			// again, these can translate into one instruction, but often don't.
		#define dct_trn8_8(x, y) \
			{ \
				uint8x8x2_t t = vtrn_u8(x, y); \
				x = t.val[0]; \
				y = t.val[1]; \
			}
		#define dct_trn8_16(x, y) \
			{ \
				uint16x4x2_t t = vtrn_u16(vreinterpret_u16_u8(x), vreinterpret_u16_u8(y)); \
				x = vreinterpret_u8_u16(t.val[0]); \
				y = vreinterpret_u8_u16(t.val[1]); \
			}
		#define dct_trn8_32(x, y) \
			{ \
				uint32x2x2_t t = vtrn_u32(vreinterpret_u32_u8(x), vreinterpret_u32_u8(y)); \
				x = vreinterpret_u8_u32(t.val[0]); \
				y = vreinterpret_u8_u32(t.val[1]); \
			}

		// sadly can't use interleaved stores here since we only write
		// 8 bytes to each scan line!

		// 8x8 8-bit transpose pass 1
		dct_trn8_8(p0, p1);
		dct_trn8_8(p2, p3);
		dct_trn8_8(p4, p5);
		dct_trn8_8(p6, p7);

		// pass 2
		dct_trn8_16(p0, p2);
		dct_trn8_16(p1, p3);
		dct_trn8_16(p4, p6);
		dct_trn8_16(p5, p7);

		// pass 3
		dct_trn8_32(p0, p4);
		dct_trn8_32(p1, p5);
		dct_trn8_32(p2, p6);
		dct_trn8_32(p3, p7);

		// store
		vst1_u8(out, p0);
		out += out_stride;
		vst1_u8(out, p1);
		out += out_stride;
		vst1_u8(out, p2);
		out += out_stride;
		vst1_u8(out, p3);
		out += out_stride;
		vst1_u8(out, p4);
		out += out_stride;
		vst1_u8(out, p5);
		out += out_stride;
		vst1_u8(out, p6);
		out += out_stride;
		vst1_u8(out, p7);

		#undef dct_trn8_8
		#undef dct_trn8_16
		#undef dct_trn8_32
	}

		#undef dct_long_mul
		#undef dct_long_mac
		#undef dct_widen
		#undef dct_wadd
		#undef dct_wsub
		#undef dct_bfly32o
		#undef dct_pass
}

	#endif // STBI_NEON

	#define STBI__MARKER_none 0xff
// if there's a pending marker from the entropy stream, return that
// otherwise, fetch from the stream and get a marker. if there's no
// marker, return 0xff, which is never a valid marker value
static stbi_uc stbi__get_marker(stbi__jpeg *j)
{
	stbi_uc x;
	if (j->marker != STBI__MARKER_none) {
		x = j->marker;
		j->marker = STBI__MARKER_none;
		return x;
	}
	x = stbi__get8(j->s);
	if (x != 0xff)
		return STBI__MARKER_none;
	while (x == 0xff)
		x = stbi__get8(j->s); // consume repeated 0xff fill bytes
	return x;
}

	// in each scan, we'll have scan_n components, and the order
	// of the components is specified by order[]
	#define STBI__RESTART(x) ((x) >= 0xd0 && (x) <= 0xd7)

// after a restart interval, stbi__jpeg_reset the entropy decoder and
// the dc prediction
static void stbi__jpeg_reset(stbi__jpeg *j)
{
	j->code_bits = 0;
	j->code_buffer = 0;
	j->nomore = 0;
	j->img_comp[0].dc_pred = j->img_comp[1].dc_pred = j->img_comp[2].dc_pred = j->img_comp[3].dc_pred = 0;
	j->marker = STBI__MARKER_none;
	j->todo = j->restart_interval ? j->restart_interval : 0x7fffffff;
	j->eob_run = 0;
	// no more than 1<<31 MCUs if no restart_interal? that's plenty safe,
	// since we don't even allow 1<<30 pixels
}

static int stbi__parse_entropy_coded_data(stbi__jpeg *z)
{
	stbi__jpeg_reset(z);
	if (!z->progressive) {
		if (z->scan_n == 1) {
			int i, j;
			STBI_SIMD_ALIGN(short, data[64]);
			int n = z->order[0];
			// non-interleaved data, we just need to process one block at a time,
			// in trivial scanline order
			// number of blocks to do just depends on how many actual "pixels" this
			// component has, independent of interleaved MCU blocking and such
			int w = (z->img_comp[n].x + 7) >> 3;
			int h = (z->img_comp[n].y + 7) >> 3;
			for (j = 0; j < h; ++j) {
				for (i = 0; i < w; ++i) {
					int ha = z->img_comp[n].ha;
					if (!stbi__jpeg_decode_block(
							z, data, z->huff_dc + z->img_comp[n].hd, z->huff_ac + ha, z->fast_ac[ha], n, z->dequant[z->img_comp[n].tq]))
						return 0;
					z->idct_block_kernel(z->img_comp[n].data + z->img_comp[n].w2 * j * 8 + i * 8, z->img_comp[n].w2, data);
					// every data block is an MCU, so countdown the restart interval
					if (--z->todo <= 0) {
						if (z->code_bits < 24)
							stbi__grow_buffer_unsafe(z);
						// if it's NOT a restart, then just bail, so we get corrupt data
						// rather than no data
						if (!STBI__RESTART(z->marker))
							return 1;
						stbi__jpeg_reset(z);
					}
				}
			}
			return 1;
		} else { // interleaved
			int i, j, k, x, y;
			STBI_SIMD_ALIGN(short, data[64]);
			for (j = 0; j < z->img_mcu_y; ++j) {
				for (i = 0; i < z->img_mcu_x; ++i) {
					// scan an interleaved mcu... process scan_n components in order
					for (k = 0; k < z->scan_n; ++k) {
						int n = z->order[k];
						// scan out an mcu's worth of this component; that's just determined
						// by the basic H and V specified for the component
						for (y = 0; y < z->img_comp[n].v; ++y) {
							for (x = 0; x < z->img_comp[n].h; ++x) {
								int x2 = (i * z->img_comp[n].h + x) * 8;
								int y2 = (j * z->img_comp[n].v + y) * 8;
								int ha = z->img_comp[n].ha;
								if (!stbi__jpeg_decode_block(
										z, data, z->huff_dc + z->img_comp[n].hd, z->huff_ac + ha, z->fast_ac[ha], n, z->dequant[z->img_comp[n].tq]))
									return 0;
								z->idct_block_kernel(z->img_comp[n].data + z->img_comp[n].w2 * y2 + x2, z->img_comp[n].w2, data);
							}
						}
					}
					// after all interleaved components, that's an interleaved MCU,
					// so now count down the restart interval
					if (--z->todo <= 0) {
						if (z->code_bits < 24)
							stbi__grow_buffer_unsafe(z);
						if (!STBI__RESTART(z->marker))
							return 1;
						stbi__jpeg_reset(z);
					}
				}
			}
			return 1;
		}
	} else {
		if (z->scan_n == 1) {
			int i, j;
			int n = z->order[0];
			// non-interleaved data, we just need to process one block at a time,
			// in trivial scanline order
			// number of blocks to do just depends on how many actual "pixels" this
			// component has, independent of interleaved MCU blocking and such
			int w = (z->img_comp[n].x + 7) >> 3;
			int h = (z->img_comp[n].y + 7) >> 3;
			for (j = 0; j < h; ++j) {
				for (i = 0; i < w; ++i) {
					short *data = z->img_comp[n].coeff + 64 * (i + j * z->img_comp[n].coeff_w);
					if (z->spec_start == 0) {
						if (!stbi__jpeg_decode_block_prog_dc(z, data, &z->huff_dc[z->img_comp[n].hd], n))
							return 0;
					} else {
						int ha = z->img_comp[n].ha;
						if (!stbi__jpeg_decode_block_prog_ac(z, data, &z->huff_ac[ha], z->fast_ac[ha]))
							return 0;
					}
					// every data block is an MCU, so countdown the restart interval
					if (--z->todo <= 0) {
						if (z->code_bits < 24)
							stbi__grow_buffer_unsafe(z);
						if (!STBI__RESTART(z->marker))
							return 1;
						stbi__jpeg_reset(z);
					}
				}
			}
			return 1;
		} else { // interleaved
			int i, j, k, x, y;
			for (j = 0; j < z->img_mcu_y; ++j) {
				for (i = 0; i < z->img_mcu_x; ++i) {
					// scan an interleaved mcu... process scan_n components in order
					for (k = 0; k < z->scan_n; ++k) {
						int n = z->order[k];
						// scan out an mcu's worth of this component; that's just determined
						// by the basic H and V specified for the component
						for (y = 0; y < z->img_comp[n].v; ++y) {
							for (x = 0; x < z->img_comp[n].h; ++x) {
								int x2 = (i * z->img_comp[n].h + x);
								int y2 = (j * z->img_comp[n].v + y);
								short *data = z->img_comp[n].coeff + 64 * (x2 + y2 * z->img_comp[n].coeff_w);
								if (!stbi__jpeg_decode_block_prog_dc(z, data, &z->huff_dc[z->img_comp[n].hd], n))
									return 0;
							}
						}
					}
					// after all interleaved components, that's an interleaved MCU,
					// so now count down the restart interval
					if (--z->todo <= 0) {
						if (z->code_bits < 24)
							stbi__grow_buffer_unsafe(z);
						if (!STBI__RESTART(z->marker))
							return 1;
						stbi__jpeg_reset(z);
					}
				}
			}
			return 1;
		}
	}
}

static void stbi__jpeg_dequantize(short *data, stbi__uint16 *dequant)
{
	int i;
	for (i = 0; i < 64; ++i)
		data[i] *= dequant[i];
}

static void stbi__jpeg_finish(stbi__jpeg *z)
{
	if (z->progressive) {
		// dequantize and idct the data
		int i, j, n;
		for (n = 0; n < z->s->img_n; ++n) {
			int w = (z->img_comp[n].x + 7) >> 3;
			int h = (z->img_comp[n].y + 7) >> 3;
			for (j = 0; j < h; ++j) {
				for (i = 0; i < w; ++i) {
					short *data = z->img_comp[n].coeff + 64 * (i + j * z->img_comp[n].coeff_w);
					stbi__jpeg_dequantize(data, z->dequant[z->img_comp[n].tq]);
					z->idct_block_kernel(z->img_comp[n].data + z->img_comp[n].w2 * j * 8 + i * 8, z->img_comp[n].w2, data);
				}
			}
		}
	}
}

static int stbi__process_marker(stbi__jpeg *z, int m)
{
	int L;
	switch (m) {
		case STBI__MARKER_none: // no marker found
			return stbi__err("expected marker", "Corrupt JPEG");

		case 0xDD: // DRI - specify restart interval
			if (stbi__get16be(z->s) != 4)
				return stbi__err("bad DRI len", "Corrupt JPEG");
			z->restart_interval = stbi__get16be(z->s);
			return 1;

		case 0xDB: // DQT - define quantization table
			L = stbi__get16be(z->s) - 2;
			while (L > 0) {
				int q = stbi__get8(z->s);
				int p = q >> 4, sixteen = (p != 0);
				int t = q & 15, i;
				if (p != 0 && p != 1)
					return stbi__err("bad DQT type", "Corrupt JPEG");
				if (t > 3)
					return stbi__err("bad DQT table", "Corrupt JPEG");

				for (i = 0; i < 64; ++i)
					z->dequant[t][stbi__jpeg_dezigzag[i]] = (stbi__uint16)(sixteen ? stbi__get16be(z->s) : stbi__get8(z->s));
				L -= (sixteen ? 129 : 65);
			}
			return L == 0;

		case 0xC4: // DHT - define huffman table
			L = stbi__get16be(z->s) - 2;
			while (L > 0) {
				stbi_uc *v;
				int sizes[16], i, n = 0;
				int q = stbi__get8(z->s);
				int tc = q >> 4;
				int th = q & 15;
				if (tc > 1 || th > 3)
					return stbi__err("bad DHT header", "Corrupt JPEG");
				for (i = 0; i < 16; ++i) {
					sizes[i] = stbi__get8(z->s);
					n += sizes[i];
				}
				L -= 17;
				if (tc == 0) {
					if (!stbi__build_huffman(z->huff_dc + th, sizes))
						return 0;
					v = z->huff_dc[th].values;
				} else {
					if (!stbi__build_huffman(z->huff_ac + th, sizes))
						return 0;
					v = z->huff_ac[th].values;
				}
				for (i = 0; i < n; ++i)
					v[i] = stbi__get8(z->s);
				if (tc != 0)
					stbi__build_fast_ac(z->fast_ac[th], z->huff_ac + th);
				L -= n;
			}
			return L == 0;
	}

	// check for comment block or APP blocks
	if ((m >= 0xE0 && m <= 0xEF) || m == 0xFE) {
		L = stbi__get16be(z->s);
		if (L < 2) {
			if (m == 0xFE)
				return stbi__err("bad COM len", "Corrupt JPEG");
			else
				return stbi__err("bad APP len", "Corrupt JPEG");
		}
		L -= 2;

		if (m == 0xE0 && L >= 5) { // JFIF APP0 segment
			static const unsigned char tag[5] = {'J', 'F', 'I', 'F', '\0'};
			int ok = 1;
			int i;
			for (i = 0; i < 5; ++i)
				if (stbi__get8(z->s) != tag[i])
					ok = 0;
			L -= 5;
			if (ok)
				z->jfif = 1;
		} else if (m == 0xEE && L >= 12) { // Adobe APP14 segment
			static const unsigned char tag[6] = {'A', 'd', 'o', 'b', 'e', '\0'};
			int ok = 1;
			int i;
			for (i = 0; i < 6; ++i)
				if (stbi__get8(z->s) != tag[i])
					ok = 0;
			L -= 6;
			if (ok) {
				stbi__get8(z->s);                            // version
				stbi__get16be(z->s);                         // flags0
				stbi__get16be(z->s);                         // flags1
				z->app14_color_transform = stbi__get8(z->s); // color transform
				L -= 6;
			}
		}

		stbi__skip(z->s, L);
		return 1;
	}

	return stbi__err("unknown marker", "Corrupt JPEG");
}

// after we see SOS
static int stbi__process_scan_header(stbi__jpeg *z)
{
	int i;
	int Ls = stbi__get16be(z->s);
	z->scan_n = stbi__get8(z->s);
	if (z->scan_n < 1 || z->scan_n > 4 || z->scan_n > (int) z->s->img_n)
		return stbi__err("bad SOS component count", "Corrupt JPEG");
	if (Ls != 6 + 2 * z->scan_n)
		return stbi__err("bad SOS len", "Corrupt JPEG");
	for (i = 0; i < z->scan_n; ++i) {
		int id = stbi__get8(z->s), which;
		int q = stbi__get8(z->s);
		for (which = 0; which < z->s->img_n; ++which)
			if (z->img_comp[which].id == id)
				break;
		if (which == z->s->img_n)
			return 0; // no match
		z->img_comp[which].hd = q >> 4;
		if (z->img_comp[which].hd > 3)
			return stbi__err("bad DC huff", "Corrupt JPEG");
		z->img_comp[which].ha = q & 15;
		if (z->img_comp[which].ha > 3)
			return stbi__err("bad AC huff", "Corrupt JPEG");
		z->order[i] = which;
	}

	{
		int aa;
		z->spec_start = stbi__get8(z->s);
		z->spec_end = stbi__get8(z->s); // should be 63, but might be 0
		aa = stbi__get8(z->s);
		z->succ_high = (aa >> 4);
		z->succ_low = (aa & 15);
		if (z->progressive) {
			if (z->spec_start > 63 || z->spec_end > 63 || z->spec_start > z->spec_end || z->succ_high > 13 || z->succ_low > 13)
				return stbi__err("bad SOS", "Corrupt JPEG");
		} else {
			if (z->spec_start != 0)
				return stbi__err("bad SOS", "Corrupt JPEG");
			if (z->succ_high != 0 || z->succ_low != 0)
				return stbi__err("bad SOS", "Corrupt JPEG");
			z->spec_end = 63;
		}
	}

	return 1;
}

static int stbi__free_jpeg_components(stbi__jpeg *z, int ncomp, int why)
{
	int i;
	for (i = 0; i < ncomp; ++i) {
		if (z->img_comp[i].raw_data) {
			STBI_FREE(z->img_comp[i].raw_data);
			z->img_comp[i].raw_data = NULL;
			z->img_comp[i].data = NULL;
		}
		if (z->img_comp[i].raw_coeff) {
			STBI_FREE(z->img_comp[i].raw_coeff);
			z->img_comp[i].raw_coeff = 0;
			z->img_comp[i].coeff = 0;
		}
		if (z->img_comp[i].linebuf) {
			STBI_FREE(z->img_comp[i].linebuf);
			z->img_comp[i].linebuf = NULL;
		}
	}
	return why;
}

static int stbi__process_frame_header(stbi__jpeg *z, int scan)
{
	stbi__context *s = z->s;
	int Lf, p, i, q, h_max = 1, v_max = 1, c;
	Lf = stbi__get16be(s);
	if (Lf < 11)
		return stbi__err("bad SOF len", "Corrupt JPEG"); // JPEG
	p = stbi__get8(s);
	if (p != 8)
		return stbi__err("only 8-bit", "JPEG format not supported: 8-bit only"); // JPEG baseline
	s->img_y = stbi__get16be(s);
	if (s->img_y == 0)
		return stbi__err("no header height", "JPEG format not supported: delayed height"); // Legal, but we don't handle it--but neither does IJG
	s->img_x = stbi__get16be(s);
	if (s->img_x == 0)
		return stbi__err("0 width", "Corrupt JPEG"); // JPEG requires
	if (s->img_y > STBI_MAX_DIMENSIONS)
		return stbi__err("too large", "Very large image (corrupt?)");
	if (s->img_x > STBI_MAX_DIMENSIONS)
		return stbi__err("too large", "Very large image (corrupt?)");
	c = stbi__get8(s);
	if (c != 3 && c != 1 && c != 4)
		return stbi__err("bad component count", "Corrupt JPEG");
	s->img_n = c;
	for (i = 0; i < c; ++i) {
		z->img_comp[i].data = NULL;
		z->img_comp[i].linebuf = NULL;
	}

	if (Lf != 8 + 3 * s->img_n)
		return stbi__err("bad SOF len", "Corrupt JPEG");

	z->rgb = 0;
	for (i = 0; i < s->img_n; ++i) {
		static const unsigned char rgb[3] = {'R', 'G', 'B'};
		z->img_comp[i].id = stbi__get8(s);
		if (s->img_n == 3 && z->img_comp[i].id == rgb[i])
			++z->rgb;
		q = stbi__get8(s);
		z->img_comp[i].h = (q >> 4);
		if (!z->img_comp[i].h || z->img_comp[i].h > 4)
			return stbi__err("bad H", "Corrupt JPEG");
		z->img_comp[i].v = q & 15;
		if (!z->img_comp[i].v || z->img_comp[i].v > 4)
			return stbi__err("bad V", "Corrupt JPEG");
		z->img_comp[i].tq = stbi__get8(s);
		if (z->img_comp[i].tq > 3)
			return stbi__err("bad TQ", "Corrupt JPEG");
	}

	if (scan != STBI__SCAN_load)
		return 1;

	if (!stbi__mad3sizes_valid(s->img_x, s->img_y, s->img_n, 0))
		return stbi__err("too large", "Image too large to decode");

	for (i = 0; i < s->img_n; ++i) {
		if (z->img_comp[i].h > h_max)
			h_max = z->img_comp[i].h;
		if (z->img_comp[i].v > v_max)
			v_max = z->img_comp[i].v;
	}

	// compute interleaved mcu info
	z->img_h_max = h_max;
	z->img_v_max = v_max;
	z->img_mcu_w = h_max * 8;
	z->img_mcu_h = v_max * 8;
	// these sizes can't be more than 17 bits
	z->img_mcu_x = (s->img_x + z->img_mcu_w - 1) / z->img_mcu_w;
	z->img_mcu_y = (s->img_y + z->img_mcu_h - 1) / z->img_mcu_h;

	for (i = 0; i < s->img_n; ++i) {
		// number of effective pixels (e.g. for non-interleaved MCU)
		z->img_comp[i].x = (s->img_x * z->img_comp[i].h + h_max - 1) / h_max;
		z->img_comp[i].y = (s->img_y * z->img_comp[i].v + v_max - 1) / v_max;
		// to simplify generation, we'll allocate enough memory to decode
		// the bogus oversized data from using interleaved MCUs and their
		// big blocks (e.g. a 16x16 iMCU on an image of width 33); we won't
		// discard the extra data until colorspace conversion
		//
		// img_mcu_x, img_mcu_y: <=17 bits; comp[i].h and .v are <=4 (checked earlier)
		// so these muls can't overflow with 32-bit ints (which we require)
		z->img_comp[i].w2 = z->img_mcu_x * z->img_comp[i].h * 8;
		z->img_comp[i].h2 = z->img_mcu_y * z->img_comp[i].v * 8;
		z->img_comp[i].coeff = 0;
		z->img_comp[i].raw_coeff = 0;
		z->img_comp[i].linebuf = NULL;
		z->img_comp[i].raw_data = stbi__malloc_mad2(z->img_comp[i].w2, z->img_comp[i].h2, 15);
		if (z->img_comp[i].raw_data == NULL)
			return stbi__free_jpeg_components(z, i + 1, stbi__err("outofmem", "Out of memory"));
		// align blocks for idct using mmx/sse
		z->img_comp[i].data = (stbi_uc *) (((size_t) z->img_comp[i].raw_data + 15) & ~15);
		if (z->progressive) {
			// w2, h2 are multiples of 8 (see above)
			z->img_comp[i].coeff_w = z->img_comp[i].w2 / 8;
			z->img_comp[i].coeff_h = z->img_comp[i].h2 / 8;
			z->img_comp[i].raw_coeff = stbi__malloc_mad3(z->img_comp[i].w2, z->img_comp[i].h2, sizeof(short), 15);
			if (z->img_comp[i].raw_coeff == NULL)
				return stbi__free_jpeg_components(z, i + 1, stbi__err("outofmem", "Out of memory"));
			z->img_comp[i].coeff = (short *) (((size_t) z->img_comp[i].raw_coeff + 15) & ~15);
		}
	}

	return 1;
}

	// use comparisons since in some cases we handle more than one case (e.g. SOF)
	#define stbi__DNL(x) ((x) == 0xdc)
	#define stbi__SOI(x) ((x) == 0xd8)
	#define stbi__EOI(x) ((x) == 0xd9)
	#define stbi__SOF(x) ((x) == 0xc0 || (x) == 0xc1 || (x) == 0xc2)
	#define stbi__SOS(x) ((x) == 0xda)

	#define stbi__SOF_progressive(x) ((x) == 0xc2)

static int stbi__decode_jpeg_header(stbi__jpeg *z, int scan)
{
	int m;
	z->jfif = 0;
	z->app14_color_transform = -1; // valid values are 0,1,2
	z->marker = STBI__MARKER_none; // initialize cached marker to empty
	m = stbi__get_marker(z);
	if (!stbi__SOI(m))
		return stbi__err("no SOI", "Corrupt JPEG");
	if (scan == STBI__SCAN_type)
		return 1;
	m = stbi__get_marker(z);
	while (!stbi__SOF(m)) {
		if (!stbi__process_marker(z, m))
			return 0;
		m = stbi__get_marker(z);
		while (m == STBI__MARKER_none) {
			// some files have extra padding after their blocks, so ok, we'll scan
			if (stbi__at_eof(z->s))
				return stbi__err("no SOF", "Corrupt JPEG");
			m = stbi__get_marker(z);
		}
	}
	z->progressive = stbi__SOF_progressive(m);
	if (!stbi__process_frame_header(z, scan))
		return 0;
	return 1;
}

// decode image to YCbCr format
static int stbi__decode_jpeg_image(stbi__jpeg *j)
{
	int m;
	for (m = 0; m < 4; m++) {
		j->img_comp[m].raw_data = NULL;
		j->img_comp[m].raw_coeff = NULL;
	}
	j->restart_interval = 0;
	if (!stbi__decode_jpeg_header(j, STBI__SCAN_load))
		return 0;
	m = stbi__get_marker(j);
	while (!stbi__EOI(m)) {
		if (stbi__SOS(m)) {
			if (!stbi__process_scan_header(j))
				return 0;
			if (!stbi__parse_entropy_coded_data(j))
				return 0;
			if (j->marker == STBI__MARKER_none) {
				// handle 0s at the end of image data from IP Kamera 9060
				while (!stbi__at_eof(j->s)) {
					int x = stbi__get8(j->s);
					if (x == 255) {
						j->marker = stbi__get8(j->s);
						break;
					}
				}
				// if we reach eof without hitting a marker, stbi__get_marker() below will fail and we'll eventually return 0
			}
		} else if (stbi__DNL(m)) {
			int Ld = stbi__get16be(j->s);
			stbi__uint32 NL = stbi__get16be(j->s);
			if (Ld != 4)
				return stbi__err("bad DNL len", "Corrupt JPEG");
			if (NL != j->s->img_y)
				return stbi__err("bad DNL height", "Corrupt JPEG");
		} else {
			if (!stbi__process_marker(j, m))
				return 0;
		}
		m = stbi__get_marker(j);
	}
	if (j->progressive)
		stbi__jpeg_finish(j);
	return 1;
}

// static jfif-centered resampling (across block boundaries)

typedef stbi_uc *(*resample_row_func)(stbi_uc *out, stbi_uc *in0, stbi_uc *in1, int w, int hs);

	#define stbi__div4(x) ((stbi_uc)((x) >> 2))

static stbi_uc *resample_row_1(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
	STBI_NOTUSED(out);
	STBI_NOTUSED(in_far);
	STBI_NOTUSED(w);
	STBI_NOTUSED(hs);
	return in_near;
}

static stbi_uc *stbi__resample_row_v_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
	// need to generate two samples vertically for every one in input
	int i;
	STBI_NOTUSED(hs);
	for (i = 0; i < w; ++i)
		out[i] = stbi__div4(3 * in_near[i] + in_far[i] + 2);
	return out;
}

static stbi_uc *stbi__resample_row_h_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
	// need to generate two samples horizontally for every one in input
	int i;
	stbi_uc *input = in_near;

	if (w == 1) {
		// if only one sample, can't do any interpolation
		out[0] = out[1] = input[0];
		return out;
	}

	out[0] = input[0];
	out[1] = stbi__div4(input[0] * 3 + input[1] + 2);
	for (i = 1; i < w - 1; ++i) {
		int n = 3 * input[i] + 2;
		out[i * 2 + 0] = stbi__div4(n + input[i - 1]);
		out[i * 2 + 1] = stbi__div4(n + input[i + 1]);
	}
	out[i * 2 + 0] = stbi__div4(input[w - 2] * 3 + input[w - 1] + 2);
	out[i * 2 + 1] = input[w - 1];

	STBI_NOTUSED(in_far);
	STBI_NOTUSED(hs);

	return out;
}

	#define stbi__div16(x) ((stbi_uc)((x) >> 4))

static stbi_uc *stbi__resample_row_hv_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
	// need to generate 2x2 samples for every one in input
	int i, t0, t1;
	if (w == 1) {
		out[0] = out[1] = stbi__div4(3 * in_near[0] + in_far[0] + 2);
		return out;
	}

	t1 = 3 * in_near[0] + in_far[0];
	out[0] = stbi__div4(t1 + 2);
	for (i = 1; i < w; ++i) {
		t0 = t1;
		t1 = 3 * in_near[i] + in_far[i];
		out[i * 2 - 1] = stbi__div16(3 * t0 + t1 + 8);
		out[i * 2] = stbi__div16(3 * t1 + t0 + 8);
	}
	out[w * 2 - 1] = stbi__div4(t1 + 2);

	STBI_NOTUSED(hs);

	return out;
}

	#if defined(STBI_SSE2) || defined(STBI_NEON)
static stbi_uc *stbi__resample_row_hv_2_simd(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
	// need to generate 2x2 samples for every one in input
	int i = 0, t0, t1;

	if (w == 1) {
		out[0] = out[1] = stbi__div4(3 * in_near[0] + in_far[0] + 2);
		return out;
	}

	t1 = 3 * in_near[0] + in_far[0];
	// process groups of 8 pixels for as long as we can.
	// note we can't handle the last pixel in a row in this loop
	// because we need to handle the filter boundary conditions.
	for (; i < ((w - 1) & ~7); i += 8) {
		#if defined(STBI_SSE2)
		// load and perform the vertical filtering pass
		// this uses 3*x + y = 4*x + (y - x)
		__m128i zero = _mm_setzero_si128();
		__m128i farb = _mm_loadl_epi64((__m128i *) (in_far + i));
		__m128i nearb = _mm_loadl_epi64((__m128i *) (in_near + i));
		__m128i farw = _mm_unpacklo_epi8(farb, zero);
		__m128i nearw = _mm_unpacklo_epi8(nearb, zero);
		__m128i diff = _mm_sub_epi16(farw, nearw);
		__m128i nears = _mm_slli_epi16(nearw, 2);
		__m128i curr = _mm_add_epi16(nears, diff); // current row

		// horizontal filter works the same based on shifted vers of current
		// row. "prev" is current row shifted right by 1 pixel; we need to
		// insert the previous pixel value (from t1).
		// "next" is current row shifted left by 1 pixel, with first pixel
		// of next block of 8 pixels added in.
		__m128i prv0 = _mm_slli_si128(curr, 2);
		__m128i nxt0 = _mm_srli_si128(curr, 2);
		__m128i prev = _mm_insert_epi16(prv0, t1, 0);
		__m128i next = _mm_insert_epi16(nxt0, 3 * in_near[i + 8] + in_far[i + 8], 7);

		// horizontal filter, polyphase implementation since it's convenient:
		// even pixels = 3*cur + prev = cur*4 + (prev - cur)
		// odd  pixels = 3*cur + next = cur*4 + (next - cur)
		// note the shared term.
		__m128i bias = _mm_set1_epi16(8);
		__m128i curs = _mm_slli_epi16(curr, 2);
		__m128i prvd = _mm_sub_epi16(prev, curr);
		__m128i nxtd = _mm_sub_epi16(next, curr);
		__m128i curb = _mm_add_epi16(curs, bias);
		__m128i even = _mm_add_epi16(prvd, curb);
		__m128i odd = _mm_add_epi16(nxtd, curb);

		// interleave even and odd pixels, then undo scaling.
		__m128i int0 = _mm_unpacklo_epi16(even, odd);
		__m128i int1 = _mm_unpackhi_epi16(even, odd);
		__m128i de0 = _mm_srli_epi16(int0, 4);
		__m128i de1 = _mm_srli_epi16(int1, 4);

		// pack and write output
		__m128i outv = _mm_packus_epi16(de0, de1);
		_mm_storeu_si128((__m128i *) (out + i * 2), outv);
		#elif defined(STBI_NEON)
		// load and perform the vertical filtering pass
		// this uses 3*x + y = 4*x + (y - x)
		uint8x8_t farb = vld1_u8(in_far + i);
		uint8x8_t nearb = vld1_u8(in_near + i);
		int16x8_t diff = vreinterpretq_s16_u16(vsubl_u8(farb, nearb));
		int16x8_t nears = vreinterpretq_s16_u16(vshll_n_u8(nearb, 2));
		int16x8_t curr = vaddq_s16(nears, diff); // current row

		// horizontal filter works the same based on shifted vers of current
		// row. "prev" is current row shifted right by 1 pixel; we need to
		// insert the previous pixel value (from t1).
		// "next" is current row shifted left by 1 pixel, with first pixel
		// of next block of 8 pixels added in.
		int16x8_t prv0 = vextq_s16(curr, curr, 7);
		int16x8_t nxt0 = vextq_s16(curr, curr, 1);
		int16x8_t prev = vsetq_lane_s16(t1, prv0, 0);
		int16x8_t next = vsetq_lane_s16(3 * in_near[i + 8] + in_far[i + 8], nxt0, 7);

		// horizontal filter, polyphase implementation since it's convenient:
		// even pixels = 3*cur + prev = cur*4 + (prev - cur)
		// odd  pixels = 3*cur + next = cur*4 + (next - cur)
		// note the shared term.
		int16x8_t curs = vshlq_n_s16(curr, 2);
		int16x8_t prvd = vsubq_s16(prev, curr);
		int16x8_t nxtd = vsubq_s16(next, curr);
		int16x8_t even = vaddq_s16(curs, prvd);
		int16x8_t odd = vaddq_s16(curs, nxtd);

		// undo scaling and round, then store with even/odd phases interleaved
		uint8x8x2_t o;
		o.val[0] = vqrshrun_n_s16(even, 4);
		o.val[1] = vqrshrun_n_s16(odd, 4);
		vst2_u8(out + i * 2, o);
		#endif

		// "previous" value for next iter
		t1 = 3 * in_near[i + 7] + in_far[i + 7];
	}

	t0 = t1;
	t1 = 3 * in_near[i] + in_far[i];
	out[i * 2] = stbi__div16(3 * t1 + t0 + 8);

	for (++i; i < w; ++i) {
		t0 = t1;
		t1 = 3 * in_near[i] + in_far[i];
		out[i * 2 - 1] = stbi__div16(3 * t0 + t1 + 8);
		out[i * 2] = stbi__div16(3 * t1 + t0 + 8);
	}
	out[w * 2 - 1] = stbi__div4(t1 + 2);

	STBI_NOTUSED(hs);

	return out;
}
	#endif

static stbi_uc *stbi__resample_row_generic(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
	// resample with nearest-neighbor
	int i, j;
	STBI_NOTUSED(in_far);
	for (i = 0; i < w; ++i)
		for (j = 0; j < hs; ++j)
			out[i * hs + j] = in_near[i];
	return out;
}

	// this is a reduced-precision calculation of YCbCr-to-RGB introduced
	// to make sure the code produces the same results in both SIMD and scalar
	#define stbi__float2fixed(x) (((int) ((x) *4096.0f + 0.5f)) << 8)
static void stbi__YCbCr_to_RGB_row(stbi_uc *out, const stbi_uc *y, const stbi_uc *pcb, const stbi_uc *pcr, int count, int step)
{
	int i;
	for (i = 0; i < count; ++i) {
		int y_fixed = (y[i] << 20) + (1 << 19); // rounding
		int r, g, b;
		int cr = pcr[i] - 128;
		int cb = pcb[i] - 128;
		r = y_fixed + cr * stbi__float2fixed(1.40200f);
		g = y_fixed + (cr * -stbi__float2fixed(0.71414f)) + ((cb * -stbi__float2fixed(0.34414f)) & 0xffff0000);
		b = y_fixed + cb * stbi__float2fixed(1.77200f);
		r >>= 20;
		g >>= 20;
		b >>= 20;
		if ((unsigned) r > 255) {
			if (r < 0)
				r = 0;
			else
				r = 255;
		}
		if ((unsigned) g > 255) {
			if (g < 0)
				g = 0;
			else
				g = 255;
		}
		if ((unsigned) b > 255) {
			if (b < 0)
				b = 0;
			else
				b = 255;
		}
		out[0] = (stbi_uc) r;
		out[1] = (stbi_uc) g;
		out[2] = (stbi_uc) b;
		out[3] = 255;
		out += step;
	}
}

	#if defined(STBI_SSE2) || defined(STBI_NEON)
static void stbi__YCbCr_to_RGB_simd(stbi_uc *out, stbi_uc const *y, stbi_uc const *pcb, stbi_uc const *pcr, int count, int step)
{
	int i = 0;

		#ifdef STBI_SSE2
	// step == 3 is pretty ugly on the final interleave, and i'm not convinced
	// it's useful in practice (you wouldn't use it for textures, for example).
	// so just accelerate step == 4 case.
	if (step == 4) {
		// this is a fairly straightforward implementation and not super-optimized.
		__m128i signflip = _mm_set1_epi8(-0x80);
		__m128i cr_const0 = _mm_set1_epi16((short) (1.40200f * 4096.0f + 0.5f));
		__m128i cr_const1 = _mm_set1_epi16(-(short) (0.71414f * 4096.0f + 0.5f));
		__m128i cb_const0 = _mm_set1_epi16(-(short) (0.34414f * 4096.0f + 0.5f));
		__m128i cb_const1 = _mm_set1_epi16((short) (1.77200f * 4096.0f + 0.5f));
		__m128i y_bias = _mm_set1_epi8((char) (unsigned char) 128);
		__m128i xw = _mm_set1_epi16(255); // alpha channel

		for (; i + 7 < count; i += 8) {
			// load
			__m128i y_bytes = _mm_loadl_epi64((__m128i *) (y + i));
			__m128i cr_bytes = _mm_loadl_epi64((__m128i *) (pcr + i));
			__m128i cb_bytes = _mm_loadl_epi64((__m128i *) (pcb + i));
			__m128i cr_biased = _mm_xor_si128(cr_bytes, signflip); // -128
			__m128i cb_biased = _mm_xor_si128(cb_bytes, signflip); // -128

			// unpack to short (and left-shift cr, cb by 8)
			__m128i yw = _mm_unpacklo_epi8(y_bias, y_bytes);
			__m128i crw = _mm_unpacklo_epi8(_mm_setzero_si128(), cr_biased);
			__m128i cbw = _mm_unpacklo_epi8(_mm_setzero_si128(), cb_biased);

			// color transform
			__m128i yws = _mm_srli_epi16(yw, 4);
			__m128i cr0 = _mm_mulhi_epi16(cr_const0, crw);
			__m128i cb0 = _mm_mulhi_epi16(cb_const0, cbw);
			__m128i cb1 = _mm_mulhi_epi16(cbw, cb_const1);
			__m128i cr1 = _mm_mulhi_epi16(crw, cr_const1);
			__m128i rws = _mm_add_epi16(cr0, yws);
			__m128i gwt = _mm_add_epi16(cb0, yws);
			__m128i bws = _mm_add_epi16(yws, cb1);
			__m128i gws = _mm_add_epi16(gwt, cr1);

			// descale
			__m128i rw = _mm_srai_epi16(rws, 4);
			__m128i bw = _mm_srai_epi16(bws, 4);
			__m128i gw = _mm_srai_epi16(gws, 4);

			// back to byte, set up for transpose
			__m128i brb = _mm_packus_epi16(rw, bw);
			__m128i gxb = _mm_packus_epi16(gw, xw);

			// transpose to interleave channels
			__m128i t0 = _mm_unpacklo_epi8(brb, gxb);
			__m128i t1 = _mm_unpackhi_epi8(brb, gxb);
			__m128i o0 = _mm_unpacklo_epi16(t0, t1);
			__m128i o1 = _mm_unpackhi_epi16(t0, t1);

			// store
			_mm_storeu_si128((__m128i *) (out + 0), o0);
			_mm_storeu_si128((__m128i *) (out + 16), o1);
			out += 32;
		}
	}
		#endif

		#ifdef STBI_NEON
	// in this version, step=3 support would be easy to add. but is there demand?
	if (step == 4) {
		// this is a fairly straightforward implementation and not super-optimized.
		uint8x8_t signflip = vdup_n_u8(0x80);
		int16x8_t cr_const0 = vdupq_n_s16((short) (1.40200f * 4096.0f + 0.5f));
		int16x8_t cr_const1 = vdupq_n_s16(-(short) (0.71414f * 4096.0f + 0.5f));
		int16x8_t cb_const0 = vdupq_n_s16(-(short) (0.34414f * 4096.0f + 0.5f));
		int16x8_t cb_const1 = vdupq_n_s16((short) (1.77200f * 4096.0f + 0.5f));

		for (; i + 7 < count; i += 8) {
			// load
			uint8x8_t y_bytes = vld1_u8(y + i);
			uint8x8_t cr_bytes = vld1_u8(pcr + i);
			uint8x8_t cb_bytes = vld1_u8(pcb + i);
			int8x8_t cr_biased = vreinterpret_s8_u8(vsub_u8(cr_bytes, signflip));
			int8x8_t cb_biased = vreinterpret_s8_u8(vsub_u8(cb_bytes, signflip));

			// expand to s16
			int16x8_t yws = vreinterpretq_s16_u16(vshll_n_u8(y_bytes, 4));
			int16x8_t crw = vshll_n_s8(cr_biased, 7);
			int16x8_t cbw = vshll_n_s8(cb_biased, 7);

			// color transform
			int16x8_t cr0 = vqdmulhq_s16(crw, cr_const0);
			int16x8_t cb0 = vqdmulhq_s16(cbw, cb_const0);
			int16x8_t cr1 = vqdmulhq_s16(crw, cr_const1);
			int16x8_t cb1 = vqdmulhq_s16(cbw, cb_const1);
			int16x8_t rws = vaddq_s16(yws, cr0);
			int16x8_t gws = vaddq_s16(vaddq_s16(yws, cb0), cr1);
			int16x8_t bws = vaddq_s16(yws, cb1);

			// undo scaling, round, convert to byte
			uint8x8x4_t o;
			o.val[0] = vqrshrun_n_s16(rws, 4);
			o.val[1] = vqrshrun_n_s16(gws, 4);
			o.val[2] = vqrshrun_n_s16(bws, 4);
			o.val[3] = vdup_n_u8(255);

			// store, interleaving r/g/b/a
			vst4_u8(out, o);
			out += 8 * 4;
		}
	}
		#endif

	for (; i < count; ++i) {
		int y_fixed = (y[i] << 20) + (1 << 19); // rounding
		int r, g, b;
		int cr = pcr[i] - 128;
		int cb = pcb[i] - 128;
		r = y_fixed + cr * stbi__float2fixed(1.40200f);
		g = y_fixed + cr * -stbi__float2fixed(0.71414f) + ((cb * -stbi__float2fixed(0.34414f)) & 0xffff0000);
		b = y_fixed + cb * stbi__float2fixed(1.77200f);
		r >>= 20;
		g >>= 20;
		b >>= 20;
		if ((unsigned) r > 255) {
			if (r < 0)
				r = 0;
			else
				r = 255;
		}
		if ((unsigned) g > 255) {
			if (g < 0)
				g = 0;
			else
				g = 255;
		}
		if ((unsigned) b > 255) {
			if (b < 0)
				b = 0;
			else
				b = 255;
		}
		out[0] = (stbi_uc) r;
		out[1] = (stbi_uc) g;
		out[2] = (stbi_uc) b;
		out[3] = 255;
		out += step;
	}
}
	#endif

// set up the kernels
static void stbi__setup_jpeg(stbi__jpeg *j)
{
	j->idct_block_kernel = stbi__idct_block;
	j->YCbCr_to_RGB_kernel = stbi__YCbCr_to_RGB_row;
	j->resample_row_hv_2_kernel = stbi__resample_row_hv_2;

	#ifdef STBI_SSE2
	if (stbi__sse2_available()) {
		j->idct_block_kernel = stbi__idct_simd;
		j->YCbCr_to_RGB_kernel = stbi__YCbCr_to_RGB_simd;
		j->resample_row_hv_2_kernel = stbi__resample_row_hv_2_simd;
	}
	#endif

	#ifdef STBI_NEON
	j->idct_block_kernel = stbi__idct_simd;
	j->YCbCr_to_RGB_kernel = stbi__YCbCr_to_RGB_simd;
	j->resample_row_hv_2_kernel = stbi__resample_row_hv_2_simd;
	#endif
}

// clean up the temporary component buffers
static void stbi__cleanup_jpeg(stbi__jpeg *j)
{
	stbi__free_jpeg_components(j, j->s->img_n, 0);
}

typedef struct {
	resample_row_func resample;
	stbi_uc *line0, *line1;
	int hs, vs;  // expansion factor in each axis
	int w_lores; // horizontal pixels pre-expansion
	int ystep;   // how far through vertical expansion we are
	int ypos;    // which pre-expansion row we're on
} stbi__resample;

// fast 0..255 * 0..255 => 0..255 rounded multiplication
static stbi_uc stbi__blinn_8x8(stbi_uc x, stbi_uc y)
{
	unsigned int t = x * y + 128;
	return (stbi_uc)((t + (t >> 8)) >> 8);
}

static stbi_uc *load_jpeg_image(stbi__jpeg *z, int *out_x, int *out_y, int *comp, int req_comp)
{
	int n, decode_n, is_rgb;
	z->s->img_n = 0; // make stbi__cleanup_jpeg safe

	// validate req_comp
	if (req_comp < 0 || req_comp > 4)
		return stbi__errpuc("bad req_comp", "Internal error");

	// load a jpeg image from whichever source, but leave in YCbCr format
	if (!stbi__decode_jpeg_image(z)) {
		stbi__cleanup_jpeg(z);
		return NULL;
	}

	// determine actual number of components to generate
	n = req_comp ? req_comp : z->s->img_n >= 3 ? 3 : 1;

	is_rgb = z->s->img_n == 3 && (z->rgb == 3 || (z->app14_color_transform == 0 && !z->jfif));

	if (z->s->img_n == 3 && n < 3 && !is_rgb)
		decode_n = 1;
	else
		decode_n = z->s->img_n;

	// resample and color-convert
	{
		int k;
		unsigned int i, j;
		stbi_uc *output;
		stbi_uc *coutput[4] = {NULL, NULL, NULL, NULL};

		stbi__resample res_comp[4];

		for (k = 0; k < decode_n; ++k) {
			stbi__resample *r = &res_comp[k];

			// allocate line buffer big enough for upsampling off the edges
			// with upsample factor of 4
			z->img_comp[k].linebuf = (stbi_uc *) stbi__malloc(z->s->img_x + 3);
			if (!z->img_comp[k].linebuf) {
				stbi__cleanup_jpeg(z);
				return stbi__errpuc("outofmem", "Out of memory");
			}

			r->hs = z->img_h_max / z->img_comp[k].h;
			r->vs = z->img_v_max / z->img_comp[k].v;
			r->ystep = r->vs >> 1;
			r->w_lores = (z->s->img_x + r->hs - 1) / r->hs;
			r->ypos = 0;
			r->line0 = r->line1 = z->img_comp[k].data;

			if (r->hs == 1 && r->vs == 1)
				r->resample = resample_row_1;
			else if (r->hs == 1 && r->vs == 2)
				r->resample = stbi__resample_row_v_2;
			else if (r->hs == 2 && r->vs == 1)
				r->resample = stbi__resample_row_h_2;
			else if (r->hs == 2 && r->vs == 2)
				r->resample = z->resample_row_hv_2_kernel;
			else
				r->resample = stbi__resample_row_generic;
		}

		// can't error after this so, this is safe
		output = (stbi_uc *) stbi__malloc_mad3(n, z->s->img_x, z->s->img_y, 1);
		if (!output) {
			stbi__cleanup_jpeg(z);
			return stbi__errpuc("outofmem", "Out of memory");
		}

		// now go ahead and resample
		for (j = 0; j < z->s->img_y; ++j) {
			stbi_uc *out = output + n * z->s->img_x * j;
			for (k = 0; k < decode_n; ++k) {
				stbi__resample *r = &res_comp[k];
				int y_bot = r->ystep >= (r->vs >> 1);
				coutput[k] = r->resample(z->img_comp[k].linebuf, y_bot ? r->line1 : r->line0, y_bot ? r->line0 : r->line1, r->w_lores, r->hs);
				if (++r->ystep >= r->vs) {
					r->ystep = 0;
					r->line0 = r->line1;
					if (++r->ypos < z->img_comp[k].y)
						r->line1 += z->img_comp[k].w2;
				}
			}
			if (n >= 3) {
				stbi_uc *y = coutput[0];
				if (z->s->img_n == 3) {
					if (is_rgb) {
						for (i = 0; i < z->s->img_x; ++i) {
							out[0] = y[i];
							out[1] = coutput[1][i];
							out[2] = coutput[2][i];
							out[3] = 255;
							out += n;
						}
					} else {
						z->YCbCr_to_RGB_kernel(out, y, coutput[1], coutput[2], z->s->img_x, n);
					}
				} else if (z->s->img_n == 4) {
					if (z->app14_color_transform == 0) { // CMYK
						for (i = 0; i < z->s->img_x; ++i) {
							stbi_uc m = coutput[3][i];
							out[0] = stbi__blinn_8x8(coutput[0][i], m);
							out[1] = stbi__blinn_8x8(coutput[1][i], m);
							out[2] = stbi__blinn_8x8(coutput[2][i], m);
							out[3] = 255;
							out += n;
						}
					} else if (z->app14_color_transform == 2) { // YCCK
						z->YCbCr_to_RGB_kernel(out, y, coutput[1], coutput[2], z->s->img_x, n);
						for (i = 0; i < z->s->img_x; ++i) {
							stbi_uc m = coutput[3][i];
							out[0] = stbi__blinn_8x8(255 - out[0], m);
							out[1] = stbi__blinn_8x8(255 - out[1], m);
							out[2] = stbi__blinn_8x8(255 - out[2], m);
							out += n;
						}
					} else { // YCbCr + alpha?  Ignore the fourth channel for now
						z->YCbCr_to_RGB_kernel(out, y, coutput[1], coutput[2], z->s->img_x, n);
					}
				} else
					for (i = 0; i < z->s->img_x; ++i) {
						out[0] = out[1] = out[2] = y[i];
						out[3] = 255; // not used if n==3
						out += n;
					}
			} else {
				if (is_rgb) {
					if (n == 1)
						for (i = 0; i < z->s->img_x; ++i)
							*out++ = stbi__compute_y(coutput[0][i], coutput[1][i], coutput[2][i]);
					else {
						for (i = 0; i < z->s->img_x; ++i, out += 2) {
							out[0] = stbi__compute_y(coutput[0][i], coutput[1][i], coutput[2][i]);
							out[1] = 255;
						}
					}
				} else if (z->s->img_n == 4 && z->app14_color_transform == 0) {
					for (i = 0; i < z->s->img_x; ++i) {
						stbi_uc m = coutput[3][i];
						stbi_uc r = stbi__blinn_8x8(coutput[0][i], m);
						stbi_uc g = stbi__blinn_8x8(coutput[1][i], m);
						stbi_uc b = stbi__blinn_8x8(coutput[2][i], m);
						out[0] = stbi__compute_y(r, g, b);
						out[1] = 255;
						out += n;
					}
				} else if (z->s->img_n == 4 && z->app14_color_transform == 2) {
					for (i = 0; i < z->s->img_x; ++i) {
						out[0] = stbi__blinn_8x8(255 - coutput[0][i], coutput[3][i]);
						out[1] = 255;
						out += n;
					}
				} else {
					stbi_uc *y = coutput[0];
					if (n == 1)
						for (i = 0; i < z->s->img_x; ++i)
							out[i] = y[i];
					else
						for (i = 0; i < z->s->img_x; ++i) {
							*out++ = y[i];
							*out++ = 255;
						}
				}
			}
		}
		stbi__cleanup_jpeg(z);
		*out_x = z->s->img_x;
		*out_y = z->s->img_y;
		if (comp)
			*comp = z->s->img_n >= 3 ? 3 : 1; // report original components, not output
		return output;
	}
}

static void *stbi__jpeg_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
	unsigned char *result;
	stbi__jpeg *j = (stbi__jpeg *) stbi__malloc(sizeof(stbi__jpeg));
	STBI_NOTUSED(ri);
	j->s = s;
	stbi__setup_jpeg(j);
	result = load_jpeg_image(j, x, y, comp, req_comp);
	STBI_FREE(j);
	return result;
}

static int stbi__jpeg_test(stbi__context *s)
{
	int r;
	stbi__jpeg *j = (stbi__jpeg *) stbi__malloc(sizeof(stbi__jpeg));
	j->s = s;
	stbi__setup_jpeg(j);
	r = stbi__decode_jpeg_header(j, STBI__SCAN_type);
	stbi__rewind(s);
	STBI_FREE(j);
	return r;
}
#endif

// public domain zlib decode    v0.2  Sean Barrett 2006-11-18
//    simple implementation
//      - all input must be provided in an upfront buffer
//      - all output is written to a single output buffer (can malloc/realloc)
//    performance
//      - fast huffman

#ifndef STBI_NO_ZLIB

	// fast-way is faster to check than jpeg huffman, but slow way is slower
	#define STBI__ZFAST_BITS 9 // accelerate all cases in default tables
	#define STBI__ZFAST_MASK ((1 << STBI__ZFAST_BITS) - 1)

// zlib-style huffman encoding
// (jpegs packs from left, zlib from right, so can't share code)
typedef struct {
	stbi__uint16 fast[1 << STBI__ZFAST_BITS];
	stbi__uint16 firstcode[16];
	int maxcode[17];
	stbi__uint16 firstsymbol[16];
	stbi_uc size[288];
	stbi__uint16 value[288];
} stbi__zhuffman;

static inline int stbi__bitreverse16(int n)
{
	n = ((n & 0xAAAA) >> 1) | ((n & 0x5555) << 1);
	n = ((n & 0xCCCC) >> 2) | ((n & 0x3333) << 2);
	n = ((n & 0xF0F0) >> 4) | ((n & 0x0F0F) << 4);
	n = ((n & 0xFF00) >> 8) | ((n & 0x00FF) << 8);
	return n;
}

static inline int stbi__bit_reverse(int v, int bits)
{
	STBI_ASSERT(bits <= 16);
	// to bit reverse n bits, reverse 16 and shift
	// e.g. 11 bits, bit reverse and shift away 5
	return stbi__bitreverse16(v) >> (16 - bits);
}

static int stbi__zbuild_huffman(stbi__zhuffman *z, const stbi_uc *sizelist, int num)
{
	int i, k = 0;
	int code, next_code[16], sizes[17];

	// DEFLATE spec for generating codes
	memset(sizes, 0, sizeof(sizes));
	memset(z->fast, 0, sizeof(z->fast));
	for (i = 0; i < num; ++i)
		++sizes[sizelist[i]];
	sizes[0] = 0;
	for (i = 1; i < 16; ++i)
		if (sizes[i] > (1 << i))
			return stbi__err("bad sizes", "Corrupt PNG");
	code = 0;
	for (i = 1; i < 16; ++i) {
		next_code[i] = code;
		z->firstcode[i] = (stbi__uint16) code;
		z->firstsymbol[i] = (stbi__uint16) k;
		code = (code + sizes[i]);
		if (sizes[i])
			if (code - 1 >= (1 << i))
				return stbi__err("bad codelengths", "Corrupt PNG");
		z->maxcode[i] = code << (16 - i); // preshift for inner loop
		code <<= 1;
		k += sizes[i];
	}
	z->maxcode[16] = 0x10000; // sentinel
	for (i = 0; i < num; ++i) {
		int s = sizelist[i];
		if (s) {
			int c = next_code[s] - z->firstcode[s] + z->firstsymbol[s];
			stbi__uint16 fastv = (stbi__uint16)((s << 9) | i);
			z->size[c] = (stbi_uc) s;
			z->value[c] = (stbi__uint16) i;
			if (s <= STBI__ZFAST_BITS) {
				int j = stbi__bit_reverse(next_code[s], s);
				while (j < (1 << STBI__ZFAST_BITS)) {
					z->fast[j] = fastv;
					j += (1 << s);
				}
			}
			++next_code[s];
		}
	}
	return 1;
}

// zlib-from-memory implementation for PNG reading
//    because PNG allows splitting the zlib stream arbitrarily,
//    and it's annoying structurally to have PNG call ZLIB call PNG,
//    we require PNG read all the IDATs and combine them into a single
//    memory buffer

typedef struct {
	stbi_uc *zbuffer, *zbuffer_end;
	int num_bits;
	stbi__uint32 code_buffer;

	char *zout;
	char *zout_start;
	char *zout_end;
	int z_expandable;

	stbi__zhuffman z_length, z_distance;
} stbi__zbuf;

static inline int stbi__zeof(stbi__zbuf *z)
{
	return (z->zbuffer >= z->zbuffer_end);
}

static inline stbi_uc stbi__zget8(stbi__zbuf *z)
{
	return stbi__zeof(z) ? 0 : *z->zbuffer++;
}

static void stbi__fill_bits(stbi__zbuf *z)
{
	do {
		if (z->code_buffer >= (1U << z->num_bits)) {
			z->zbuffer = z->zbuffer_end; /* treat this as EOF so we fail. */
			return;
		}
		z->code_buffer |= (unsigned int) stbi__zget8(z) << z->num_bits;
		z->num_bits += 8;
	} while (z->num_bits <= 24);
}

static inline unsigned int stbi__zreceive(stbi__zbuf *z, int n)
{
	unsigned int k;
	if (z->num_bits < n)
		stbi__fill_bits(z);
	k = z->code_buffer & ((1 << n) - 1);
	z->code_buffer >>= n;
	z->num_bits -= n;
	return k;
}

static int stbi__zhuffman_decode_slowpath(stbi__zbuf *a, stbi__zhuffman *z)
{
	int b, s, k;
	// not resolved by fast table, so compute it the slow way
	// use jpeg approach, which requires MSbits at top
	k = stbi__bit_reverse(a->code_buffer, 16);
	for (s = STBI__ZFAST_BITS + 1;; ++s)
		if (k < z->maxcode[s])
			break;
	if (s >= 16)
		return -1; // invalid code!
	// code size is s, so:
	b = (k >> (16 - s)) - z->firstcode[s] + z->firstsymbol[s];
	if (b >= (int) sizeof(z->size))
		return -1; // some data was corrupt somewhere!
	if (z->size[b] != s)
		return -1; // was originally an assert, but report failure instead.
	a->code_buffer >>= s;
	a->num_bits -= s;
	return z->value[b];
}

static inline int stbi__zhuffman_decode(stbi__zbuf *a, stbi__zhuffman *z)
{
	int b, s;
	if (a->num_bits < 16) {
		if (stbi__zeof(a)) {
			return -1; /* report error for unexpected end of data. */
		}
		stbi__fill_bits(a);
	}
	b = z->fast[a->code_buffer & STBI__ZFAST_MASK];
	if (b) {
		s = b >> 9;
		a->code_buffer >>= s;
		a->num_bits -= s;
		return b & 511;
	}
	return stbi__zhuffman_decode_slowpath(a, z);
}

static int stbi__zexpand(stbi__zbuf *z, char *zout, int n) // need to make room for n bytes
{
	char *q;
	unsigned int cur, limit, old_limit;
	z->zout = zout;
	if (!z->z_expandable)
		return stbi__err("output buffer limit", "Corrupt PNG");
	cur = (unsigned int) (z->zout - z->zout_start);
	limit = old_limit = (unsigned) (z->zout_end - z->zout_start);
	if (UINT_MAX - cur < (unsigned) n)
		return stbi__err("outofmem", "Out of memory");
	while (cur + n > limit) {
		if (limit > UINT_MAX / 2)
			return stbi__err("outofmem", "Out of memory");
		limit *= 2;
	}
	q = (char *) STBI_REALLOC_SIZED(z->zout_start, old_limit, limit);
	STBI_NOTUSED(old_limit);
	if (q == NULL)
		return stbi__err("outofmem", "Out of memory");
	z->zout_start = q;
	z->zout = q + cur;
	z->zout_end = q + limit;
	return 1;
}

static const int stbi__zlength_base[31] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};

static const int stbi__zlength_extra[31] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0};

static const int stbi__zdist_base[32] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0, 0};

static const int stbi__zdist_extra[32] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int stbi__parse_huffman_block(stbi__zbuf *a)
{
	char *zout = a->zout;
	for (;;) {
		int z = stbi__zhuffman_decode(a, &a->z_length);
		if (z < 256) {
			if (z < 0)
				return stbi__err("bad huffman code", "Corrupt PNG"); // error in huffman codes
			if (zout >= a->zout_end) {
				if (!stbi__zexpand(a, zout, 1))
					return 0;
				zout = a->zout;
			}
			*zout++ = (char) z;
		} else {
			stbi_uc *p;
			int len, dist;
			if (z == 256) {
				a->zout = zout;
				return 1;
			}
			z -= 257;
			len = stbi__zlength_base[z];
			if (stbi__zlength_extra[z])
				len += stbi__zreceive(a, stbi__zlength_extra[z]);
			z = stbi__zhuffman_decode(a, &a->z_distance);
			if (z < 0)
				return stbi__err("bad huffman code", "Corrupt PNG");
			dist = stbi__zdist_base[z];
			if (stbi__zdist_extra[z])
				dist += stbi__zreceive(a, stbi__zdist_extra[z]);
			if (zout - a->zout_start < dist)
				return stbi__err("bad dist", "Corrupt PNG");
			if (zout + len > a->zout_end) {
				if (!stbi__zexpand(a, zout, len))
					return 0;
				zout = a->zout;
			}
			p = (stbi_uc *) (zout - dist);
			if (dist == 1) { // run of one byte; common in images.
				stbi_uc v = *p;
				if (len) {
					do
						*zout++ = v;
					while (--len);
				}
			} else {
				if (len) {
					do
						*zout++ = *p++;
					while (--len);
				}
			}
		}
	}
}

static int stbi__compute_huffman_codes(stbi__zbuf *a)
{
	static const stbi_uc length_dezigzag[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
	stbi__zhuffman z_codelength;
	stbi_uc lencodes[286 + 32 + 137]; //padding for maximum single op
	stbi_uc codelength_sizes[19];
	int i, n;

	int hlit = stbi__zreceive(a, 5) + 257;
	int hdist = stbi__zreceive(a, 5) + 1;
	int hclen = stbi__zreceive(a, 4) + 4;
	int ntot = hlit + hdist;

	memset(codelength_sizes, 0, sizeof(codelength_sizes));
	for (i = 0; i < hclen; ++i) {
		int s = stbi__zreceive(a, 3);
		codelength_sizes[length_dezigzag[i]] = (stbi_uc) s;
	}
	if (!stbi__zbuild_huffman(&z_codelength, codelength_sizes, 19))
		return 0;

	n = 0;
	while (n < ntot) {
		int c = stbi__zhuffman_decode(a, &z_codelength);
		if (c < 0 || c >= 19)
			return stbi__err("bad codelengths", "Corrupt PNG");
		if (c < 16)
			lencodes[n++] = (stbi_uc) c;
		else {
			stbi_uc fill = 0;
			if (c == 16) {
				c = stbi__zreceive(a, 2) + 3;
				if (n == 0)
					return stbi__err("bad codelengths", "Corrupt PNG");
				fill = lencodes[n - 1];
			} else if (c == 17) {
				c = stbi__zreceive(a, 3) + 3;
			} else if (c == 18) {
				c = stbi__zreceive(a, 7) + 11;
			} else {
				return stbi__err("bad codelengths", "Corrupt PNG");
			}
			if (ntot - n < c)
				return stbi__err("bad codelengths", "Corrupt PNG");
			memset(lencodes + n, fill, c);
			n += c;
		}
	}
	if (n != ntot)
		return stbi__err("bad codelengths", "Corrupt PNG");
	if (!stbi__zbuild_huffman(&a->z_length, lencodes, hlit))
		return 0;
	if (!stbi__zbuild_huffman(&a->z_distance, lencodes + hlit, hdist))
		return 0;
	return 1;
}

static int stbi__parse_uncompressed_block(stbi__zbuf *a)
{
	stbi_uc header[4];
	int len, nlen, k;
	if (a->num_bits & 7)
		stbi__zreceive(a, a->num_bits & 7); // discard
	// drain the bit-packed data into header
	k = 0;
	while (a->num_bits > 0) {
		header[k++] = (stbi_uc)(a->code_buffer & 255); // suppress MSVC run-time check
		a->code_buffer >>= 8;
		a->num_bits -= 8;
	}
	if (a->num_bits < 0)
		return stbi__err("zlib corrupt", "Corrupt PNG");
	// now fill header the normal way
	while (k < 4)
		header[k++] = stbi__zget8(a);
	len = header[1] * 256 + header[0];
	nlen = header[3] * 256 + header[2];
	if (nlen != (len ^ 0xffff))
		return stbi__err("zlib corrupt", "Corrupt PNG");
	if (a->zbuffer + len > a->zbuffer_end)
		return stbi__err("read past buffer", "Corrupt PNG");
	if (a->zout + len > a->zout_end)
		if (!stbi__zexpand(a, a->zout, len))
			return 0;
	memcpy(a->zout, a->zbuffer, len);
	a->zbuffer += len;
	a->zout += len;
	return 1;
}

static int stbi__parse_zlib_header(stbi__zbuf *a)
{
	int cmf = stbi__zget8(a);
	int cm = cmf & 15;
	/* int cinfo = cmf >> 4; */
	int flg = stbi__zget8(a);
	if (stbi__zeof(a))
		return stbi__err("bad zlib header", "Corrupt PNG"); // zlib spec
	if ((cmf * 256 + flg) % 31 != 0)
		return stbi__err("bad zlib header", "Corrupt PNG"); // zlib spec
	if (flg & 32)
		return stbi__err("no preset dict", "Corrupt PNG"); // preset dictionary not allowed in png
	if (cm != 8)
		return stbi__err("bad compression", "Corrupt PNG"); // DEFLATE required for png
	// window = 1 << (8 + cinfo)... but who cares, we fully buffer output
	return 1;
}

static const stbi_uc stbi__zdefault_length[288] = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8};
static const stbi_uc stbi__zdefault_distance[32] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
/*
Init algorithm:
{
   int i;   // use <= to match clearly with spec
   for (i=0; i <= 143; ++i)     stbi__zdefault_length[i]   = 8;
   for (   ; i <= 255; ++i)     stbi__zdefault_length[i]   = 9;
   for (   ; i <= 279; ++i)     stbi__zdefault_length[i]   = 7;
   for (   ; i <= 287; ++i)     stbi__zdefault_length[i]   = 8;

   for (i=0; i <=  31; ++i)     stbi__zdefault_distance[i] = 5;
}
*/

static int stbi__parse_zlib(stbi__zbuf *a, int parse_header)
{
	int final, type;
	if (parse_header)
		if (!stbi__parse_zlib_header(a))
			return 0;
	a->num_bits = 0;
	a->code_buffer = 0;
	do {
		final = stbi__zreceive(a, 1);
		type = stbi__zreceive(a, 2);
		if (type == 0) {
			if (!stbi__parse_uncompressed_block(a))
				return 0;
		} else if (type == 3) {
			return 0;
		} else {
			if (type == 1) {
				// use fixed code lengths
				if (!stbi__zbuild_huffman(&a->z_length, stbi__zdefault_length, 288))
					return 0;
				if (!stbi__zbuild_huffman(&a->z_distance, stbi__zdefault_distance, 32))
					return 0;
			} else {
				if (!stbi__compute_huffman_codes(a))
					return 0;
			}
			if (!stbi__parse_huffman_block(a))
				return 0;
		}
	} while (!final);
	return 1;
}

static int stbi__do_zlib(stbi__zbuf *a, char *obuf, int olen, int exp, int parse_header)
{
	a->zout_start = obuf;
	a->zout = obuf;
	a->zout_end = obuf + olen;
	a->z_expandable = exp;

	return stbi__parse_zlib(a, parse_header);
}

static char *stbi_zlib_decode_malloc_guesssize_headerflag(const char *buffer, int len, int initial_size, int *outlen, int parse_header)
{
	stbi__zbuf a;
	char *p = (char *) stbi__malloc(initial_size);
	if (p == NULL)
		return NULL;
	a.zbuffer = (stbi_uc *) buffer;
	a.zbuffer_end = (stbi_uc *) buffer + len;
	if (stbi__do_zlib(&a, p, initial_size, 1, parse_header)) {
		if (outlen)
			*outlen = (int) (a.zout - a.zout_start);
		return a.zout_start;
	} else {
		STBI_FREE(a.zout_start);
		return NULL;
	}
}

/* TODO This may be useful
static int stbi_zlib_decode_buffer(char *obuffer, int olen, char const *ibuffer, int ilen)
{
   stbi__zbuf a;
   a.zbuffer = (stbi_uc *) ibuffer;
   a.zbuffer_end = (stbi_uc *) ibuffer + ilen;
   if (stbi__do_zlib(&a, obuffer, olen, 0, 1))
      return (int) (a.zout - a.zout_start);
   else
      return -1;
}
*/
#endif

// public domain "baseline" PNG decoder   v0.10  Sean Barrett 2006-11-18
//    simple implementation
//      - only 8-bit samples
//      - no CRC checking
//      - allocates lots of intermediate memory
//        - avoids problem of streaming data between subsystems
//        - avoids explicit window management
//    performance
//      - uses stb_zlib, a PD zlib implementation with fast huffman decoding

#ifndef STBI_NO_PNG
typedef struct {
	stbi__uint32 length;
	stbi__uint32 type;
} stbi__pngchunk;

static stbi__pngchunk stbi__get_chunk_header(stbi__context *s)
{
	stbi__pngchunk c;
	c.length = stbi__get32be(s);
	c.type = stbi__get32be(s);
	return c;
}

static int stbi__check_png_header(stbi__context *s)
{
	static const stbi_uc png_sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
	int i;
	for (i = 0; i < 8; ++i)
		if (stbi__get8(s) != png_sig[i])
			return stbi__err("bad png sig", "Not a PNG");
	return 1;
}

typedef struct {
	stbi__context *s;
	stbi_uc *idata, *expanded, *out;
	int depth;
} stbi__png;


enum {
	STBI__F_none = 0,
	STBI__F_sub = 1,
	STBI__F_up = 2,
	STBI__F_avg = 3,
	STBI__F_paeth = 4,
	// synthetic filters used for first scanline to avoid needing a dummy row of 0s
	STBI__F_avg_first,
	STBI__F_paeth_first
};

static stbi_uc first_row_filter[5] = {STBI__F_none, STBI__F_sub, STBI__F_none, STBI__F_avg_first, STBI__F_paeth_first};

static int stbi__paeth(int a, int b, int c)
{
	int p = a + b - c;
	int pa = abs(p - a);
	int pb = abs(p - b);
	int pc = abs(p - c);
	if (pa <= pb && pa <= pc)
		return a;
	if (pb <= pc)
		return b;
	return c;
}

static const stbi_uc stbi__depth_scale_table[9] = {0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01};

// create the png data from post-deflated data
static int stbi__create_png_image_raw(stbi__png *a, stbi_uc *raw, stbi__uint32 raw_len, int out_n, stbi__uint32 x, stbi__uint32 y, int depth, int color)
{
	int bytes = (depth == 16 ? 2 : 1);
	stbi__context *s = a->s;
	stbi__uint32 i, j, stride = x * out_n * bytes;
	stbi__uint32 img_len, img_width_bytes;
	int k;
	int img_n = s->img_n; // copy it into a local for later

	int output_bytes = out_n * bytes;
	int filter_bytes = img_n * bytes;
	int width = x;

	STBI_ASSERT(out_n == s->img_n || out_n == s->img_n + 1);
	a->out = (stbi_uc *) stbi__malloc_mad3(x, y, output_bytes, 0); // extra bytes to write off the end into
	if (!a->out)
		return stbi__err("outofmem", "Out of memory");

	if (!stbi__mad3sizes_valid(img_n, x, depth, 7))
		return stbi__err("too large", "Corrupt PNG");
	img_width_bytes = (((img_n * x * depth) + 7) >> 3);
	img_len = (img_width_bytes + 1) * y;

	// we used to check for exact match between raw_len and img_len on non-interlaced PNGs,
	// but issue #276 reported a PNG in the wild that had extra data at the end (all zeros),
	// so just check for raw_len < img_len always.
	if (raw_len < img_len)
		return stbi__err("not enough pixels", "Corrupt PNG");

	for (j = 0; j < y; ++j) {
		stbi_uc *cur = a->out + stride * j;
		stbi_uc *prior;
		int filter = *raw++;

		if (filter > 4)
			return stbi__err("invalid filter", "Corrupt PNG");

		if (depth < 8) {
			if (img_width_bytes > x)
				return stbi__err("invalid width", "Corrupt PNG");
			cur += x * out_n - img_width_bytes; // store output to the rightmost img_len bytes, so we can decode in place
			filter_bytes = 1;
			width = img_width_bytes;
		}
		prior = cur - stride; // bugfix: need to compute this after 'cur +=' computation above

		// if first row, use special filter that doesn't sample previous row
		if (j == 0)
			filter = first_row_filter[filter];

		// handle first byte explicitly
		for (k = 0; k < filter_bytes; ++k) {
			switch (filter) {
				case STBI__F_none:
					cur[k] = raw[k];
					break;
				case STBI__F_sub:
					cur[k] = raw[k];
					break;
				case STBI__F_up:
					cur[k] = STBI__BYTECAST(raw[k] + prior[k]);
					break;
				case STBI__F_avg:
					cur[k] = STBI__BYTECAST(raw[k] + (prior[k] >> 1));
					break;
				case STBI__F_paeth:
					cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(0, prior[k], 0));
					break;
				case STBI__F_avg_first:
					cur[k] = raw[k];
					break;
				case STBI__F_paeth_first:
					cur[k] = raw[k];
					break;
			}
		}

		if (depth == 8) {
			if (img_n != out_n)
				cur[img_n] = 255; // first pixel
			raw += img_n;
			cur += out_n;
			prior += out_n;
		} else if (depth == 16) {
			if (img_n != out_n) {
				cur[filter_bytes] = 255;     // first pixel top byte
				cur[filter_bytes + 1] = 255; // first pixel bottom byte
			}
			raw += filter_bytes;
			cur += output_bytes;
			prior += output_bytes;
		} else {
			raw += 1;
			cur += 1;
			prior += 1;
		}

		// this is a little gross, so that we don't switch per-pixel or per-component
		if (depth < 8 || img_n == out_n) {
			int nk = (width - 1) * filter_bytes;
	#define STBI__CASE(f) \
		case f: \
			for (k = 0; k < nk; ++k)
			switch (filter) {
				// "none" filter turns into a memcpy here; make that explicit.
				case STBI__F_none:
					memcpy(cur, raw, nk);
					break;
					STBI__CASE(STBI__F_sub)
					{
						cur[k] = STBI__BYTECAST(raw[k] + cur[k - filter_bytes]);
					}
					break;
					STBI__CASE(STBI__F_up)
					{
						cur[k] = STBI__BYTECAST(raw[k] + prior[k]);
					}
					break;
					STBI__CASE(STBI__F_avg)
					{
						cur[k] = STBI__BYTECAST(raw[k] + ((prior[k] + cur[k - filter_bytes]) >> 1));
					}
					break;
					STBI__CASE(STBI__F_paeth)
					{
						cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k - filter_bytes], prior[k], prior[k - filter_bytes]));
					}
					break;
					STBI__CASE(STBI__F_avg_first)
					{
						cur[k] = STBI__BYTECAST(raw[k] + (cur[k - filter_bytes] >> 1));
					}
					break;
					STBI__CASE(STBI__F_paeth_first)
					{
						cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k - filter_bytes], 0, 0));
					}
					break;
			}
	#undef STBI__CASE
			raw += nk;
		} else {
			STBI_ASSERT(img_n + 1 == out_n);
	#define STBI__CASE(f) \
		case f: \
			for (i = x - 1; i >= 1; --i, cur[filter_bytes] = 255, raw += filter_bytes, cur += output_bytes, prior += output_bytes) \
				for (k = 0; k < filter_bytes; ++k)
			switch (filter) {
				STBI__CASE(STBI__F_none)
				{
					cur[k] = raw[k];
				}
				break;
				STBI__CASE(STBI__F_sub)
				{
					cur[k] = STBI__BYTECAST(raw[k] + cur[k - output_bytes]);
				}
				break;
				STBI__CASE(STBI__F_up)
				{
					cur[k] = STBI__BYTECAST(raw[k] + prior[k]);
				}
				break;
				STBI__CASE(STBI__F_avg)
				{
					cur[k] = STBI__BYTECAST(raw[k] + ((prior[k] + cur[k - output_bytes]) >> 1));
				}
				break;
				STBI__CASE(STBI__F_paeth)
				{
					cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k - output_bytes], prior[k], prior[k - output_bytes]));
				}
				break;
				STBI__CASE(STBI__F_avg_first)
				{
					cur[k] = STBI__BYTECAST(raw[k] + (cur[k - output_bytes] >> 1));
				}
				break;
				STBI__CASE(STBI__F_paeth_first)
				{
					cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k - output_bytes], 0, 0));
				}
				break;
			}
	#undef STBI__CASE

			// the loop above sets the high byte of the pixels' alpha, but for
			// 16 bit png files we also need the low byte set. we'll do that here.
			if (depth == 16) {
				cur = a->out + stride * j; // start at the beginning of the row again
				for (i = 0; i < x; ++i, cur += output_bytes) {
					cur[filter_bytes + 1] = 255;
				}
			}
		}
	}

	// we make a separate pass to expand bits to pixels; for performance,
	// this could run two scanlines behind the above code, so it won't
	// intefere with filtering but will still be in the cache.
	if (depth < 8) {
		for (j = 0; j < y; ++j) {
			stbi_uc *cur = a->out + stride * j;
			stbi_uc *in = a->out + stride * j + x * out_n - img_width_bytes;
			// unpack 1/2/4-bit into a 8-bit buffer. allows us to keep the common 8-bit path optimal at minimal cost for 1/2/4-bit
			// png guarante byte alignment, if width is not multiple of 8/4/2 we'll decode dummy trailing data that will be skipped in the later loop
			stbi_uc scale = (color == 0) ? stbi__depth_scale_table[depth] : 1; // scale grayscale values to 0..255 range

			// note that the final byte might overshoot and write more data than desired.
			// we can allocate enough data that this never writes out of memory, but it
			// could also overwrite the next scanline. can it overwrite non-empty data
			// on the next scanline? yes, consider 1-pixel-wide scanlines with 1-bit-per-pixel.
			// so we need to explicitly clamp the final ones

			if (depth == 4) {
				for (k = x * img_n; k >= 2; k -= 2, ++in) {
					*cur++ = scale * ((*in >> 4));
					*cur++ = scale * ((*in) & 0x0f);
				}
				if (k > 0)
					*cur++ = scale * ((*in >> 4));
			} else if (depth == 2) {
				for (k = x * img_n; k >= 4; k -= 4, ++in) {
					*cur++ = scale * ((*in >> 6));
					*cur++ = scale * ((*in >> 4) & 0x03);
					*cur++ = scale * ((*in >> 2) & 0x03);
					*cur++ = scale * ((*in) & 0x03);
				}
				if (k > 0)
					*cur++ = scale * ((*in >> 6));
				if (k > 1)
					*cur++ = scale * ((*in >> 4) & 0x03);
				if (k > 2)
					*cur++ = scale * ((*in >> 2) & 0x03);
			} else if (depth == 1) {
				for (k = x * img_n; k >= 8; k -= 8, ++in) {
					*cur++ = scale * ((*in >> 7));
					*cur++ = scale * ((*in >> 6) & 0x01);
					*cur++ = scale * ((*in >> 5) & 0x01);
					*cur++ = scale * ((*in >> 4) & 0x01);
					*cur++ = scale * ((*in >> 3) & 0x01);
					*cur++ = scale * ((*in >> 2) & 0x01);
					*cur++ = scale * ((*in >> 1) & 0x01);
					*cur++ = scale * ((*in) & 0x01);
				}
				if (k > 0)
					*cur++ = scale * ((*in >> 7));
				if (k > 1)
					*cur++ = scale * ((*in >> 6) & 0x01);
				if (k > 2)
					*cur++ = scale * ((*in >> 5) & 0x01);
				if (k > 3)
					*cur++ = scale * ((*in >> 4) & 0x01);
				if (k > 4)
					*cur++ = scale * ((*in >> 3) & 0x01);
				if (k > 5)
					*cur++ = scale * ((*in >> 2) & 0x01);
				if (k > 6)
					*cur++ = scale * ((*in >> 1) & 0x01);
			}
			if (img_n != out_n) {
				int q;
				// insert alpha = 255
				cur = a->out + stride * j;
				if (img_n == 1) {
					for (q = x - 1; q >= 0; --q) {
						cur[q * 2 + 1] = 255;
						cur[q * 2 + 0] = cur[q];
					}
				} else {
					STBI_ASSERT(img_n == 3);
					for (q = x - 1; q >= 0; --q) {
						cur[q * 4 + 3] = 255;
						cur[q * 4 + 2] = cur[q * 3 + 2];
						cur[q * 4 + 1] = cur[q * 3 + 1];
						cur[q * 4 + 0] = cur[q * 3 + 0];
					}
				}
			}
		}
	} else if (depth == 16) {
		// force the image data from big-endian to platform-native.
		// this is done in a separate pass due to the decoding relying
		// on the data being untouched, but could probably be done
		// per-line during decode if care is taken.
		stbi_uc *cur = a->out;
		stbi__uint16 *cur16 = (stbi__uint16 *) cur;

		for (i = 0; i < x * y * out_n; ++i, cur16++, cur += 2) {
			*cur16 = (cur[0] << 8) | cur[1];
		}
	}

	return 1;
}

static int stbi__create_png_image(stbi__png *a, stbi_uc *image_data, stbi__uint32 image_data_len, int out_n, int depth, int color, int interlaced)
{
	int bytes = (depth == 16 ? 2 : 1);
	int out_bytes = out_n * bytes;
	stbi_uc *final;
	int p;
	if (!interlaced)
		return stbi__create_png_image_raw(a, image_data, image_data_len, out_n, a->s->img_x, a->s->img_y, depth, color);

	// de-interlacing
	final = (stbi_uc *) stbi__malloc_mad3(a->s->img_x, a->s->img_y, out_bytes, 0);
	for (p = 0; p < 7; ++p) {
		int xorig[] = {0, 4, 0, 2, 0, 1, 0};
		int yorig[] = {0, 0, 4, 0, 2, 0, 1};
		int xspc[] = {8, 8, 4, 4, 2, 2, 1};
		int yspc[] = {8, 8, 8, 4, 4, 2, 2};
		int i, j, x, y;
		// pass1_x[4] = 0, pass1_x[5] = 1, pass1_x[12] = 1
		x = (a->s->img_x - xorig[p] + xspc[p] - 1) / xspc[p];
		y = (a->s->img_y - yorig[p] + yspc[p] - 1) / yspc[p];
		if (x && y) {
			stbi__uint32 img_len = ((((a->s->img_n * x * depth) + 7) >> 3) + 1) * y;
			if (!stbi__create_png_image_raw(a, image_data, image_data_len, out_n, x, y, depth, color)) {
				STBI_FREE(final);
				return 0;
			}
			for (j = 0; j < y; ++j) {
				for (i = 0; i < x; ++i) {
					int out_y = j * yspc[p] + yorig[p];
					int out_x = i * xspc[p] + xorig[p];
					memcpy(final + out_y * a->s->img_x * out_bytes + out_x * out_bytes, a->out + (j * x + i) * out_bytes, out_bytes);
				}
			}
			STBI_FREE(a->out);
			image_data += img_len;
			image_data_len -= img_len;
		}
	}
	a->out = final;

	return 1;
}

static int stbi__compute_transparency(stbi__png *z, stbi_uc tc[3], int out_n)
{
	stbi__context *s = z->s;
	stbi__uint32 i, pixel_count = s->img_x * s->img_y;
	stbi_uc *p = z->out;

	// compute color-based transparency, assuming we've
	// already got 255 as the alpha value in the output
	STBI_ASSERT(out_n == 2 || out_n == 4);

	if (out_n == 2) {
		for (i = 0; i < pixel_count; ++i) {
			p[1] = (p[0] == tc[0] ? 0 : 255);
			p += 2;
		}
	} else {
		for (i = 0; i < pixel_count; ++i) {
			if (p[0] == tc[0] && p[1] == tc[1] && p[2] == tc[2])
				p[3] = 0;
			p += 4;
		}
	}
	return 1;
}

static int stbi__compute_transparency16(stbi__png *z, stbi__uint16 tc[3], int out_n)
{
	stbi__context *s = z->s;
	stbi__uint32 i, pixel_count = s->img_x * s->img_y;
	stbi__uint16 *p = (stbi__uint16 *) z->out;

	// compute color-based transparency, assuming we've
	// already got 65535 as the alpha value in the output
	STBI_ASSERT(out_n == 2 || out_n == 4);

	if (out_n == 2) {
		for (i = 0; i < pixel_count; ++i) {
			p[1] = (p[0] == tc[0] ? 0 : 65535);
			p += 2;
		}
	} else {
		for (i = 0; i < pixel_count; ++i) {
			if (p[0] == tc[0] && p[1] == tc[1] && p[2] == tc[2])
				p[3] = 0;
			p += 4;
		}
	}
	return 1;
}

static int stbi__expand_png_palette(stbi__png *a, stbi_uc *palette, int len, int pal_img_n)
{
	stbi__uint32 i, pixel_count = a->s->img_x * a->s->img_y;
	stbi_uc *p, *temp_out, *orig = a->out;

	p = (stbi_uc *) stbi__malloc_mad2(pixel_count, pal_img_n, 0);
	if (p == NULL)
		return stbi__err("outofmem", "Out of memory");

	// between here and free(out) below, exitting would leak
	temp_out = p;

	if (pal_img_n == 3) {
		for (i = 0; i < pixel_count; ++i) {
			int n = orig[i] * 4;
			p[0] = palette[n];
			p[1] = palette[n + 1];
			p[2] = palette[n + 2];
			p += 3;
		}
	} else {
		for (i = 0; i < pixel_count; ++i) {
			int n = orig[i] * 4;
			p[0] = palette[n];
			p[1] = palette[n + 1];
			p[2] = palette[n + 2];
			p[3] = palette[n + 3];
			p += 4;
		}
	}
	STBI_FREE(a->out);
	a->out = temp_out;

	STBI_NOTUSED(len);

	return 1;
}

static int stbi__unpremultiply_on_load = 0;
static int stbi__de_iphone_flag = 0;

static void stbi__de_iphone(stbi__png *z)
{
	stbi__context *s = z->s;
	stbi__uint32 i, pixel_count = s->img_x * s->img_y;
	stbi_uc *p = z->out;

	if (s->img_out_n == 3) { // convert bgr to rgb
		for (i = 0; i < pixel_count; ++i) {
			stbi_uc t = p[0];
			p[0] = p[2];
			p[2] = t;
			p += 3;
		}
	} else {
		STBI_ASSERT(s->img_out_n == 4);
		if (stbi__unpremultiply_on_load) {
			// convert bgr to rgb and unpremultiply
			for (i = 0; i < pixel_count; ++i) {
				stbi_uc a = p[3];
				stbi_uc t = p[0];
				if (a) {
					stbi_uc half = a / 2;
					p[0] = (p[2] * 255 + half) / a;
					p[1] = (p[1] * 255 + half) / a;
					p[2] = (t * 255 + half) / a;
				} else {
					p[0] = p[2];
					p[2] = t;
				}
				p += 4;
			}
		} else {
			// convert bgr to rgb
			for (i = 0; i < pixel_count; ++i) {
				stbi_uc t = p[0];
				p[0] = p[2];
				p[2] = t;
				p += 4;
			}
		}
	}
}

	#define STBI__PNG_TYPE(a, b, c, d) (((unsigned) (a) << 24) + ((unsigned) (b) << 16) + ((unsigned) (c) << 8) + (unsigned) (d))

static int stbi__parse_png_file(stbi__png *z, int scan, int req_comp)
{
	stbi_uc palette[1024], pal_img_n = 0;
	stbi_uc has_trans = 0, tc[3] = {0};
	stbi__uint16 tc16[3];
	stbi__uint32 ioff = 0, idata_limit = 0, i, pal_len = 0;
	int first = 1, k, interlace = 0, color = 0, is_iphone = 0;
	stbi__context *s = z->s;

	z->expanded = NULL;
	z->idata = NULL;
	z->out = NULL;

	if (!stbi__check_png_header(s))
		return 0;

	if (scan == STBI__SCAN_type)
		return 1;

	for (;;) {
		stbi__pngchunk c = stbi__get_chunk_header(s);
		switch (c.type) {
			case STBI__PNG_TYPE('C', 'g', 'B', 'I'):
				is_iphone = 1;
				stbi__skip(s, c.length);
				break;
			case STBI__PNG_TYPE('I', 'H', 'D', 'R'): {
				int comp, filter;
				if (!first)
					return stbi__err("multiple IHDR", "Corrupt PNG");
				first = 0;
				if (c.length != 13)
					return stbi__err("bad IHDR len", "Corrupt PNG");
				s->img_x = stbi__get32be(s);
				s->img_y = stbi__get32be(s);
				if (s->img_y > STBI_MAX_DIMENSIONS)
					return stbi__err("too large", "Very large image (corrupt?)");
				if (s->img_x > STBI_MAX_DIMENSIONS)
					return stbi__err("too large", "Very large image (corrupt?)");
				z->depth = stbi__get8(s);
				if (z->depth != 1 && z->depth != 2 && z->depth != 4 && z->depth != 8 && z->depth != 16)
					return stbi__err("1/2/4/8/16-bit only", "PNG not supported: 1/2/4/8/16-bit only");
				color = stbi__get8(s);
				if (color > 6)
					return stbi__err("bad ctype", "Corrupt PNG");
				if (color == 3 && z->depth == 16)
					return stbi__err("bad ctype", "Corrupt PNG");
				if (color == 3)
					pal_img_n = 3;
				else if (color & 1)
					return stbi__err("bad ctype", "Corrupt PNG");
				comp = stbi__get8(s);
				if (comp)
					return stbi__err("bad comp method", "Corrupt PNG");
				filter = stbi__get8(s);
				if (filter)
					return stbi__err("bad filter method", "Corrupt PNG");
				interlace = stbi__get8(s);
				if (interlace > 1)
					return stbi__err("bad interlace method", "Corrupt PNG");
				if (!s->img_x || !s->img_y)
					return stbi__err("0-pixel image", "Corrupt PNG");
				if (!pal_img_n) {
					s->img_n = (color & 2 ? 3 : 1) + (color & 4 ? 1 : 0);
					if ((1 << 30) / s->img_x / s->img_n < s->img_y)
						return stbi__err("too large", "Image too large to decode");
					if (scan == STBI__SCAN_header)
						return 1;
				} else {
					// if paletted, then pal_n is our final components, and
					// img_n is # components to decompress/filter.
					s->img_n = 1;
					if ((1 << 30) / s->img_x / 4 < s->img_y)
						return stbi__err("too large", "Corrupt PNG");
					// if SCAN_header, have to scan to see if we have a tRNS
				}
				break;
			}

			case STBI__PNG_TYPE('P', 'L', 'T', 'E'): {
				if (first)
					return stbi__err("first not IHDR", "Corrupt PNG");
				if (c.length > 256 * 3)
					return stbi__err("invalid PLTE", "Corrupt PNG");
				pal_len = c.length / 3;
				if (pal_len * 3 != c.length)
					return stbi__err("invalid PLTE", "Corrupt PNG");
				for (i = 0; i < pal_len; ++i) {
					palette[i * 4 + 0] = stbi__get8(s);
					palette[i * 4 + 1] = stbi__get8(s);
					palette[i * 4 + 2] = stbi__get8(s);
					palette[i * 4 + 3] = 255;
				}
				break;
			}

			case STBI__PNG_TYPE('t', 'R', 'N', 'S'): {
				if (first)
					return stbi__err("first not IHDR", "Corrupt PNG");
				if (z->idata)
					return stbi__err("tRNS after IDAT", "Corrupt PNG");
				if (pal_img_n) {
					if (scan == STBI__SCAN_header) {
						s->img_n = 4;
						return 1;
					}
					if (pal_len == 0)
						return stbi__err("tRNS before PLTE", "Corrupt PNG");
					if (c.length > pal_len)
						return stbi__err("bad tRNS len", "Corrupt PNG");
					pal_img_n = 4;
					for (i = 0; i < c.length; ++i)
						palette[i * 4 + 3] = stbi__get8(s);
				} else {
					if (!(s->img_n & 1))
						return stbi__err("tRNS with alpha", "Corrupt PNG");
					if (c.length != (stbi__uint32) s->img_n * 2)
						return stbi__err("bad tRNS len", "Corrupt PNG");
					has_trans = 1;
					if (z->depth == 16) {
						for (k = 0; k < s->img_n; ++k)
							tc16[k] = (stbi__uint16) stbi__get16be(s); // copy the values as-is
					} else {
						for (k = 0; k < s->img_n; ++k)
							tc[k] = (stbi_uc)(stbi__get16be(s) & 255) * stbi__depth_scale_table[z->depth]; // non 8-bit images will be larger
					}
				}
				break;
			}

			case STBI__PNG_TYPE('I', 'D', 'A', 'T'): {
				if (first)
					return stbi__err("first not IHDR", "Corrupt PNG");
				if (pal_img_n && !pal_len)
					return stbi__err("no PLTE", "Corrupt PNG");
				if (scan == STBI__SCAN_header) {
					s->img_n = pal_img_n;
					return 1;
				}
				if ((int) (ioff + c.length) < (int) ioff)
					return 0;
				if (ioff + c.length > idata_limit) {
					stbi__uint32 idata_limit_old = idata_limit;
					stbi_uc *p;
					if (idata_limit == 0)
						idata_limit = c.length > 4096 ? c.length : 4096;
					while (ioff + c.length > idata_limit)
						idata_limit *= 2;
					STBI_NOTUSED(idata_limit_old);
					p = (stbi_uc *) STBI_REALLOC_SIZED(z->idata, idata_limit_old, idata_limit);
					if (p == NULL)
						return stbi__err("outofmem", "Out of memory");
					z->idata = p;
				}
				if (!stbi__getn(s, z->idata + ioff, c.length))
					return stbi__err("outofdata", "Corrupt PNG");
				ioff += c.length;
				break;
			}

			case STBI__PNG_TYPE('I', 'E', 'N', 'D'): {
				stbi__uint32 raw_len, bpl;
				if (first)
					return stbi__err("first not IHDR", "Corrupt PNG");
				if (scan != STBI__SCAN_load)
					return 1;
				if (z->idata == NULL)
					return stbi__err("no IDAT", "Corrupt PNG");
				// initial guess for decoded data size to avoid unnecessary reallocs
				bpl = (s->img_x * z->depth + 7) / 8; // bytes per line, per component
				raw_len = bpl * s->img_y * s->img_n /* pixels */ + s->img_y /* filter mode per row */;
				z->expanded = (stbi_uc *) stbi_zlib_decode_malloc_guesssize_headerflag((char *) z->idata, ioff, raw_len, (int *) &raw_len, !is_iphone);
				if (z->expanded == NULL)
					return 0; // zlib should set error
				STBI_FREE(z->idata);
				z->idata = NULL;
				if ((req_comp == s->img_n + 1 && req_comp != 3 && !pal_img_n) || has_trans)
					s->img_out_n = s->img_n + 1;
				else
					s->img_out_n = s->img_n;
				if (!stbi__create_png_image(z, z->expanded, raw_len, s->img_out_n, z->depth, color, interlace))
					return 0;
				if (has_trans) {
					if (z->depth == 16) {
						if (!stbi__compute_transparency16(z, tc16, s->img_out_n))
							return 0;
					} else {
						if (!stbi__compute_transparency(z, tc, s->img_out_n))
							return 0;
					}
				}
				if (is_iphone && stbi__de_iphone_flag && s->img_out_n > 2)
					stbi__de_iphone(z);
				if (pal_img_n) {
					// pal_img_n == 3 or 4
					s->img_n = pal_img_n; // record the actual colors we had
					s->img_out_n = pal_img_n;
					if (req_comp >= 3)
						s->img_out_n = req_comp;
					if (!stbi__expand_png_palette(z, palette, pal_len, s->img_out_n))
						return 0;
				} else if (has_trans) {
					// non-paletted image with tRNS -> source image has (constant) alpha
					++s->img_n;
				}
				STBI_FREE(z->expanded);
				z->expanded = NULL;
				// end of PNG chunk, read and skip CRC
				stbi__get32be(s);
				return 1;
			}

			default:
				// if critical, fail
				if (first)
					return stbi__err("first not IHDR", "Corrupt PNG");
				if ((c.type & (1 << 29)) == 0) {
	#ifndef STBI_NO_FAILURE_STRINGS
					// not threadsafe
					static char invalid_chunk[] = "XXXX PNG chunk not known";
					invalid_chunk[0] = STBI__BYTECAST(c.type >> 24);
					invalid_chunk[1] = STBI__BYTECAST(c.type >> 16);
					invalid_chunk[2] = STBI__BYTECAST(c.type >> 8);
					invalid_chunk[3] = STBI__BYTECAST(c.type >> 0);
	#endif
					return stbi__err(invalid_chunk, "PNG not supported: unknown PNG chunk type");
				}
				stbi__skip(s, c.length);
				break;
		}
		// end of PNG chunk, read and skip CRC
		stbi__get32be(s);
	}
}

static void *stbi__do_png(stbi__png *p, int *x, int *y, int *n, int req_comp, stbi__result_info *ri)
{
	void *result = NULL;
	if (req_comp < 0 || req_comp > 4)
		return stbi__errpuc("bad req_comp", "Internal error");
	if (stbi__parse_png_file(p, STBI__SCAN_load, req_comp)) {
		if (p->depth <= 8)
			ri->bits_per_channel = 8;
		else if (p->depth == 16)
			ri->bits_per_channel = 16;
		else
			return stbi__errpuc("bad bits_per_channel", "PNG not supported: unsupported color depth");
		result = p->out;
		p->out = NULL;
		if (req_comp && req_comp != p->s->img_out_n) {
			if (ri->bits_per_channel == 8)
				result = stbi__convert_format((unsigned char *) result, p->s->img_out_n, req_comp, p->s->img_x, p->s->img_y);
			else
				result = stbi__convert_format16((stbi__uint16 *) result, p->s->img_out_n, req_comp, p->s->img_x, p->s->img_y);
			p->s->img_out_n = req_comp;
			if (result == NULL)
				return result;
		}
		*x = p->s->img_x;
		*y = p->s->img_y;
		if (n)
			*n = p->s->img_n;
	}
	STBI_FREE(p->out);
	p->out = NULL;
	STBI_FREE(p->expanded);
	p->expanded = NULL;
	STBI_FREE(p->idata);
	p->idata = NULL;

	return result;
}

static void *stbi__png_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
	stbi__png p;
	p.s = s;
	return stbi__do_png(&p, x, y, comp, req_comp, ri);
}

static int stbi__png_test(stbi__context *s)
{
	int r;
	r = stbi__check_png_header(s);
	stbi__rewind(s);
	return r;
}
#endif

// *************************************************************************************************
// GIF loader -- public domain by Jean-Marc Lienher -- simplified/shrunk by stb

#ifndef STBI_NO_GIF
typedef struct {
	stbi__int16 prefix;
	stbi_uc first;
	stbi_uc suffix;
} stbi__gif_lzw;

typedef struct {
	int w, h;
	stbi_uc *out;        // output buffer (always 4 components)
	stbi_uc *background; // The current "background" as far as a gif is concerned
	stbi_uc *history;
	int flags, bgindex, ratio, transparent, eflags;
	stbi_uc pal[256][4];
	stbi_uc lpal[256][4];
	stbi__gif_lzw codes[8192];
	stbi_uc *color_table;
	int parse, step;
	int lflags;
	int start_x, start_y;
	int max_x, max_y;
	int cur_x, cur_y;
	int line_size;
	int delay;
} stbi__gif;

static int stbi__gif_test_raw(stbi__context *s)
{
	int sz;
	if (stbi__get8(s) != 'G' || stbi__get8(s) != 'I' || stbi__get8(s) != 'F' || stbi__get8(s) != '8')
		return 0;
	sz = stbi__get8(s);
	if (sz != '9' && sz != '7')
		return 0;
	if (stbi__get8(s) != 'a')
		return 0;
	return 1;
}

static int stbi__gif_test(stbi__context *s)
{
	int r = stbi__gif_test_raw(s);
	stbi__rewind(s);
	return r;
}

static void stbi__gif_parse_colortable(stbi__context *s, stbi_uc pal[256][4], int num_entries, int transp)
{
	int i;
	for (i = 0; i < num_entries; ++i) {
		pal[i][2] = stbi__get8(s);
		pal[i][1] = stbi__get8(s);
		pal[i][0] = stbi__get8(s);
		pal[i][3] = transp == i ? 0 : 255;
	}
}

static int stbi__gif_header(stbi__context *s, stbi__gif *g, int *comp, int is_info)
{
	stbi_uc version;
	if (stbi__get8(s) != 'G' || stbi__get8(s) != 'I' || stbi__get8(s) != 'F' || stbi__get8(s) != '8')
		return stbi__err("not GIF", "Corrupt GIF");

	version = stbi__get8(s);
	if (version != '7' && version != '9')
		return stbi__err("not GIF", "Corrupt GIF");
	if (stbi__get8(s) != 'a')
		return stbi__err("not GIF", "Corrupt GIF");

	stbi__g_failure_reason = "";
	g->w = stbi__get16le(s);
	g->h = stbi__get16le(s);
	g->flags = stbi__get8(s);
	g->bgindex = stbi__get8(s);
	g->ratio = stbi__get8(s);
	g->transparent = -1;

	if (g->w > STBI_MAX_DIMENSIONS)
		return stbi__err("too large", "Very large image (corrupt?)");
	if (g->h > STBI_MAX_DIMENSIONS)
		return stbi__err("too large", "Very large image (corrupt?)");

	if (comp != 0)
		*comp = 4; // can't actually tell whether it's 3 or 4 until we parse the comments

	if (is_info)
		return 1;

	if (g->flags & 0x80)
		stbi__gif_parse_colortable(s, g->pal, 2 << (g->flags & 7), -1);

	return 1;
}

static void stbi__out_gif_code(stbi__gif *g, stbi__uint16 code)
{
	stbi_uc *p, *c;
	int idx;

	// recurse to decode the prefixes, since the linked-list is backwards,
	// and working backwards through an interleaved image would be nasty
	if (g->codes[code].prefix >= 0)
		stbi__out_gif_code(g, g->codes[code].prefix);

	if (g->cur_y >= g->max_y)
		return;

	idx = g->cur_x + g->cur_y;
	p = &g->out[idx];
	g->history[idx / 4] = 1;

	c = &g->color_table[g->codes[code].suffix * 4];
	if (c[3] > 128) { // don't render transparent pixels;
		p[0] = c[2];
		p[1] = c[1];
		p[2] = c[0];
		p[3] = c[3];
	}
	g->cur_x += 4;

	if (g->cur_x >= g->max_x) {
		g->cur_x = g->start_x;
		g->cur_y += g->step;

		while (g->cur_y >= g->max_y && g->parse > 0) {
			g->step = (1 << g->parse) * g->line_size;
			g->cur_y = g->start_y + (g->step >> 1);
			--g->parse;
		}
	}
}

static stbi_uc *stbi__process_gif_raster(stbi__context *s, stbi__gif *g)
{
	stbi_uc lzw_cs;
	stbi__int32 len, init_code;
	stbi__uint32 first;
	stbi__int32 codesize, codemask, avail, oldcode, bits, valid_bits, clear;
	stbi__gif_lzw *p;

	lzw_cs = stbi__get8(s);
	if (lzw_cs > 12)
		return NULL;
	clear = 1 << lzw_cs;
	first = 1;
	codesize = lzw_cs + 1;
	codemask = (1 << codesize) - 1;
	bits = 0;
	valid_bits = 0;
	for (init_code = 0; init_code < clear; init_code++) {
		g->codes[init_code].prefix = -1;
		g->codes[init_code].first = (stbi_uc) init_code;
		g->codes[init_code].suffix = (stbi_uc) init_code;
	}

	// support no starting clear code
	avail = clear + 2;
	oldcode = -1;

	len = 0;
	for (;;) {
		if (valid_bits < codesize) {
			if (len == 0) {
				len = stbi__get8(s); // start new block
				if (len == 0)
					return g->out;
			}
			--len;
			bits |= (stbi__int32) stbi__get8(s) << valid_bits;
			valid_bits += 8;
		} else {
			stbi__int32 code = bits & codemask;
			bits >>= codesize;
			valid_bits -= codesize;
			// @OPTIMIZE: is there some way we can accelerate the non-clear path?
			if (code == clear) { // clear code
				codesize = lzw_cs + 1;
				codemask = (1 << codesize) - 1;
				avail = clear + 2;
				oldcode = -1;
				first = 0;
			} else if (code == clear + 1) { // end of stream code
				stbi__skip(s, len);
				while ((len = stbi__get8(s)) > 0)
					stbi__skip(s, len);
				return g->out;
			} else if (code <= avail) {
				if (first) {
					return stbi__errpuc("no clear code", "Corrupt GIF");
				}

				if (oldcode >= 0) {
					p = &g->codes[avail++];
					if (avail > 8192) {
						return stbi__errpuc("too many codes", "Corrupt GIF");
					}

					p->prefix = (stbi__int16) oldcode;
					p->first = g->codes[oldcode].first;
					p->suffix = (code == avail) ? p->first : g->codes[code].first;
				} else if (code == avail)
					return stbi__errpuc("illegal code in raster", "Corrupt GIF");

				stbi__out_gif_code(g, (stbi__uint16) code);

				if ((avail & codemask) == 0 && avail <= 0x0FFF) {
					codesize++;
					codemask = (1 << codesize) - 1;
				}

				oldcode = code;
			} else {
				return stbi__errpuc("illegal code in raster", "Corrupt GIF");
			}
		}
	}
}

// this function is designed to support animated gifs, although stb_image doesn't support it
// two back is the image from two frames ago, used for a very specific disposal format
static stbi_uc *stbi__gif_load_next(stbi__context *s, stbi__gif *g, int *comp, int req_comp, stbi_uc *two_back)
{
	int dispose;
	int first_frame;
	int pi;
	int pcount;
	STBI_NOTUSED(req_comp);

	// on first frame, any non-written pixels get the background colour (non-transparent)
	first_frame = 0;
	if (g->out == 0) {
		if (!stbi__gif_header(s, g, comp, 0))
			return 0; // stbi__g_failure_reason set by stbi__gif_header
		if (!stbi__mad3sizes_valid(4, g->w, g->h, 0))
			return stbi__errpuc("too large", "GIF image is too large");
		pcount = g->w * g->h;
		g->out = (stbi_uc *) stbi__malloc(4 * pcount);
		g->background = (stbi_uc *) stbi__malloc(4 * pcount);
		g->history = (stbi_uc *) stbi__malloc(pcount);
		if (!g->out || !g->background || !g->history)
			return stbi__errpuc("outofmem", "Out of memory");

		// image is treated as "transparent" at the start - ie, nothing overwrites the current background;
		// background colour is only used for pixels that are not rendered first frame, after that "background"
		// color refers to the color that was there the previous frame.
		memset(g->out, 0x00, 4 * pcount);
		memset(g->background, 0x00, 4 * pcount); // state of the background (starts transparent)
		memset(g->history, 0x00, pcount);        // pixels that were affected previous frame
		first_frame = 1;
	} else {
		// second frame - how do we dispose of the previous one?
		dispose = (g->eflags & 0x1C) >> 2;
		pcount = g->w * g->h;

		if ((dispose == 3) && (two_back == 0)) {
			dispose = 2; // if I don't have an image to revert back to, default to the old background
		}

		if (dispose == 3) { // use previous graphic
			for (pi = 0; pi < pcount; ++pi) {
				if (g->history[pi]) {
					memcpy(&g->out[pi * 4], &two_back[pi * 4], 4);
				}
			}
		} else if (dispose == 2) {
			// restore what was changed last frame to background before that frame;
			for (pi = 0; pi < pcount; ++pi) {
				if (g->history[pi]) {
					memcpy(&g->out[pi * 4], &g->background[pi * 4], 4);
				}
			}
		} else {
			// This is a non-disposal case eithe way, so just
			// leave the pixels as is, and they will become the new background
			// 1: do not dispose
			// 0:  not specified.
		}

		// background is what out is after the undoing of the previou frame;
		memcpy(g->background, g->out, 4 * g->w * g->h);
	}

	// clear my history;
	memset(g->history, 0x00, g->w * g->h); // pixels that were affected previous frame

	for (;;) {
		int tag = stbi__get8(s);
		switch (tag) {
			case 0x2C: /* Image Descriptor */
			{
				stbi__int32 x, y, w, h;
				stbi_uc *o;

				x = stbi__get16le(s);
				y = stbi__get16le(s);
				w = stbi__get16le(s);
				h = stbi__get16le(s);
				if (((x + w) > (g->w)) || ((y + h) > (g->h)))
					return stbi__errpuc("bad Image Descriptor", "Corrupt GIF");

				g->line_size = g->w * 4;
				g->start_x = x * 4;
				g->start_y = y * g->line_size;
				g->max_x = g->start_x + w * 4;
				g->max_y = g->start_y + h * g->line_size;
				g->cur_x = g->start_x;
				g->cur_y = g->start_y;

				// if the width of the specified rectangle is 0, that means
				// we may not see *any* pixels or the image is malformed;
				// to make sure this is caught, move the current y down to
				// max_y (which is what out_gif_code checks).
				if (w == 0)
					g->cur_y = g->max_y;

				g->lflags = stbi__get8(s);

				if (g->lflags & 0x40) {
					g->step = 8 * g->line_size; // first interlaced spacing
					g->parse = 3;
				} else {
					g->step = g->line_size;
					g->parse = 0;
				}

				if (g->lflags & 0x80) {
					stbi__gif_parse_colortable(s, g->lpal, 2 << (g->lflags & 7), g->eflags & 0x01 ? g->transparent : -1);
					g->color_table = (stbi_uc *) g->lpal;
				} else if (g->flags & 0x80) {
					g->color_table = (stbi_uc *) g->pal;
				} else
					return stbi__errpuc("missing color table", "Corrupt GIF");

				o = stbi__process_gif_raster(s, g);
				if (!o)
					return NULL;

				// if this was the first frame,
				pcount = g->w * g->h;
				if (first_frame && (g->bgindex > 0)) {
					// if first frame, any pixel not drawn to gets the background color
					for (pi = 0; pi < pcount; ++pi) {
						if (g->history[pi] == 0) {
							g->pal[g->bgindex][3] = 255; // just in case it was made transparent, undo that; It will be reset next frame if need be;
							memcpy(&g->out[pi * 4], &g->pal[g->bgindex], 4);
						}
					}
				}

				return o;
			}

			case 0x21: // Comment Extension.
			{
				int len;
				int ext = stbi__get8(s);
				if (ext == 0xF9) { // Graphic Control Extension.
					len = stbi__get8(s);
					if (len == 4) {
						g->eflags = stbi__get8(s);
						g->delay = 10 * stbi__get16le(s); // delay - 1/100th of a second, saving as 1/1000ths.

						// unset old transparent
						if (g->transparent >= 0) {
							g->pal[g->transparent][3] = 255;
						}
						if (g->eflags & 0x01) {
							g->transparent = stbi__get8(s);
							if (g->transparent >= 0) {
								g->pal[g->transparent][3] = 0;
							}
						} else {
							// don't need transparent
							stbi__skip(s, 1);
							g->transparent = -1;
						}
					} else {
						stbi__skip(s, len);
						break;
					}
				}
				while ((len = stbi__get8(s)) != 0) {
					stbi__skip(s, len);
				}
				break;
			}

			case 0x3B:                // gif stream termination code
				return (stbi_uc *) s; // using '1' causes warning on some compilers

			default:
				return stbi__errpuc("unknown code", "Corrupt GIF");
		}
	}
}

static void *stbi__gif_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
	stbi_uc *u = 0;
	stbi__gif g;
	memset(&g, 0, sizeof(g));
	STBI_NOTUSED(ri);

	u = stbi__gif_load_next(s, &g, comp, req_comp, 0);
	if (u == (stbi_uc *) s)
		u = 0; // end of animated gif marker
	if (u) {
		*x = g.w;
		*y = g.h;

		// moved conversion to after successful load so that the same
		// can be done for multiple frames.
		if (req_comp && req_comp != 4)
			u = stbi__convert_format(u, 4, req_comp, g.w, g.h);
	} else if (g.out) {
		// if there was an error and we allocated an image buffer, free it!
		STBI_FREE(g.out);
	}

	// free buffers needed for multiple frame loading;
	STBI_FREE(g.history);
	STBI_FREE(g.background);

	return u;
}
#endif
