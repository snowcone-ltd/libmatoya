// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "jnih.h"

void *MTY_CompressImage(MTY_ImageCompression method, const void *input, uint32_t width,
	uint32_t height, size_t *outputSize)
{
	*outputSize = 0;

	return NULL;
}

void *MTY_DecompressImage(const void *input, size_t size, uint32_t *width, uint32_t *height)
{
	JNIEnv *env = MTY_GetJNIEnv();

	uint32_t *image = NULL;

	// Compressed input
	jbyteArray jinput = mty_jni_dup(env, input, size);
	jobject source = mty_jni_static_obj(env, "android/graphics/ImageDecoder", "createSource",
		"([B)Landroid/graphics/ImageDecoder$Source;", jinput);
	if (!mty_jni_ok(env))
		goto except;

	// Decode
	jobject bitmap = mty_jni_static_obj(env, "android/graphics/ImageDecoder", "decodeBitmap",
		"(Landroid/graphics/ImageDecoder$Source;)Landroid/graphics/Bitmap;", source);
	if (!mty_jni_ok(env))
		goto except;

	// Find ARGB enumeration object
	jclass cfg_cls = (*env)->FindClass(env, "android/graphics/Bitmap$Config");
	jfieldID rgba_field = (*env)->GetStaticFieldID(env, cfg_cls, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
	jobject rgba_enum = (*env)->GetStaticObjectField(env, cfg_cls, rgba_field);
	mty_jni_free(env, cfg_cls);

	// Copy from hardware -> software
	bitmap = mty_jni_obj(env, bitmap, "copy", "(Landroid/graphics/Bitmap$Config;Z)Landroid/graphics/Bitmap;",
		rgba_enum, false);
	if (!mty_jni_ok(env))
		goto except;

	// Allocate, iterate and swizzle
	*width = mty_jni_int(env, bitmap, "getWidth", "()I");
	*height = mty_jni_int(env, bitmap, "getHeight", "()I");

	image = MTY_Alloc(*width * *height, 4);

	jclass bm_cls = (*env)->GetObjectClass(env, bitmap);
	jmethodID mid = (*env)->GetMethodID(env, bm_cls, "getPixel", "(II)I");
	mty_jni_free(env, bm_cls);

	for (uint32_t y = 0; y < *height; y++) {
		for (uint32_t x = 0; x < *width; x++) {
			uint32_t p = (*env)->CallIntMethod(env, bitmap, mid, x, y);
			image[y * *width + x] = (p & 0xFF) << 16 | (p & 0xFF00) | (p & 0xFF0000) >> 16 | (p & 0xFF000000);
		}
	}

	except:

	mty_jni_free(env, jinput);

	return image;
}

void *MTY_GetProgramIcon(const char *path, uint32_t *width, uint32_t *height)
{
	return NULL;
}
