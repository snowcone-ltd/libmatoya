// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <CoreGraphics/CoreGraphics.h>
#include <CoreServices/CoreServices.h>
#include <ImageIO/ImageIO.h>

void *MTY_CompressImage(MTY_ImageCompression method, const void *input, uint32_t width,
	uint32_t height, size_t *outputSize)
{
	void *output = NULL;

	CGDataProviderRef provider = NULL;
	CGColorSpaceRef cs = NULL;
	CGImageRef rgba = NULL;
	CFMutableDataRef data = NULL;
	CGImageDestinationRef dest = NULL;

	// Source RGBA
	provider = CGDataProviderCreateWithData(NULL, input, width * height * 4, NULL);
	if (!provider)
		goto except;

	cs = CGColorSpaceCreateDeviceRGB();
	CGBitmapInfo bi = kCGImageAlphaFirst | kCGImageByteOrder32Little;

	rgba = CGImageCreate(width, height, 8, 32, width * 4, cs, bi, provider,
		NULL, false, kCGRenderingIntentDefault);
	if (!rgba)
		goto except;

	// Destination PNG
	CFStringRef type = NULL;

	switch (method) {
		case MTY_IMAGE_COMPRESSION_PNG:
			type = kUTTypePNG;
			break;
		case MTY_IMAGE_COMPRESSION_JPEG:
			type = kUTTypeJPEG;
			break;
		default:
			goto except;
	}

	data = CFDataCreateMutable(NULL, 0);
	dest = CGImageDestinationCreateWithData(data, type, 1, NULL);

	CGImageDestinationAddImage(dest, rgba, NULL);

	if (!CGImageDestinationFinalize(dest))
		goto except;

	const UInt8 *raw_buf = CFDataGetBytePtr(data);
	*outputSize = CFDataGetLength(data);

	output = MTY_Alloc(*outputSize, 1);
	memcpy(output, raw_buf, *outputSize);

	except:

	if (dest)
		CFRelease(dest);

	if (data)
		CFRelease(data);

	if (rgba)
		CGImageRelease(rgba);

	if (cs)
		CGColorSpaceRelease(cs);

	if (provider)
		CGDataProviderRelease(provider);

	return output;
}

void *MTY_DecompressImage(const void *input, size_t size, uint32_t *width, uint32_t *height)
{
	void *image = NULL;
	CGDataProviderRef provider = NULL;
	CGImageRef cgimg = NULL;

	if (size < 3)
		goto except;

	provider = CGDataProviderCreateWithData(NULL, input, size, NULL);
	if (!provider)
		goto except;

	const uint8_t *input8 = input;

	// JPEG
	if (input8[0] == 0xFF && input8[1] == 0xD8 && input8[2] == 0xFF) {
		cgimg = CGImageCreateWithJPEGDataProvider(provider, NULL, false, kCGRenderingIntentDefault);

	// PNG
	} else if (input8[0] == 0x89 && input8[1] == 0x50 && input8[2] == 0x4E) {
		cgimg = CGImageCreateWithPNGDataProvider(provider, NULL, false, kCGRenderingIntentDefault);
	}

	if (!cgimg)
		goto except;

	*width = CGImageGetWidth(cgimg);
	*height = CGImageGetHeight(cgimg);

	CFDataRef raw = CGDataProviderCopyData(CGImageGetDataProvider(cgimg));
	if (!raw)
		goto except;

	const UInt8 *raw_buf = CFDataGetBytePtr(raw);
	CFIndex raw_size = CFDataGetLength(raw);

	image = MTY_Alloc(raw_size, 1);
	memcpy(image, raw_buf, raw_size);

	CFRelease(raw);

	except:

	if (cgimg)
		CGImageRelease(cgimg);

	if (provider)
		CGDataProviderRelease(provider);

	return image;
}

void MTY_DecompressImageAsync(const void *input, size_t size, MTY_ImageFunc func, void *opaque)
{
	uint32_t w = 0;
	uint32_t h = 0;

	void *image = MTY_DecompressImage(input, size, &w, &h);

	func(image, w, h, opaque);
}

void *MTY_GetProgramIcon(const char *path, uint32_t *width, uint32_t *height)
{
	return NULL;
}
