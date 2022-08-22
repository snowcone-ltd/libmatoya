// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#define COBJMACROS
#include <wincodec.h>
#include <shlwapi.h>
#include <shellapi.h>

void *MTY_CompressImage(MTY_ImageCompression method, const void *input, uint32_t width,
	uint32_t height, size_t *outputSize)
{
	void *cmp = NULL;
	IWICImagingFactory *factory = NULL;
	IWICBitmapEncoder *encoder = NULL;
	IWICBitmapFrameEncode *frame = NULL;
	IStream *stream = NULL;

	HRESULT ce = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	HRESULT e = CoCreateInstance(&CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, &factory);
	if (e != S_OK) {
		MTY_Log("'CoCreateInstance' failed with HRESULT 0x%X", e);
		goto except;
	}

	GUID format = {0};
	switch (method) {
		case MTY_IMAGE_COMPRESSION_PNG: format = GUID_ContainerFormatPng;  break;
		case MTY_IMAGE_COMPRESSION_JPEG: format = GUID_ContainerFormatJpeg; break;
		default:
			MTY_Log("MTY_ImageCompression method %d not supported", method);
			goto except;
	}

	e = IWICImagingFactory_CreateEncoder(factory, &format, NULL, &encoder);
	if (e != S_OK) {
		MTY_Log("'IWICImagingFactory_CreateEncoder' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = CreateStreamOnHGlobal(NULL, TRUE, &stream);
	if (e != S_OK) {
		MTY_Log("'CreateStreamOnHGlobal' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapEncoder_Initialize' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapDecoder_CreateNewFrame' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapFrameEncode_Initialize(frame, NULL);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapFrameEncode_Initialize' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapFrameEncode_SetSize(frame, width, height);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapFrameEncode_SetSize' failed with HRESULT 0x%X", e);
		goto except;
	}

	WICPixelFormatGUID wic_format = GUID_WICPixelFormat32bppBGRA;
	e = IWICBitmapFrameEncode_SetPixelFormat(frame, &wic_format);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapFrameEncode_SetPixelFormat' failed with HRESULT 0x%X", e);
		goto except;
	}

	if (memcmp(&wic_format, &GUID_WICPixelFormat32bppBGRA, sizeof(WICPixelFormatGUID))) {
		MTY_Log("GUID_WICPixelFormat32bppRGBA is not available for MTY_ImageCompression method %d", method);
		goto except;
	}

	UINT stride = width * 4;
	UINT rgba_size = stride * height;
	e = IWICBitmapFrameEncode_WritePixels(frame, height, stride, rgba_size, (BYTE *) input);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapFrameEncode_WritePixels' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapFrameEncode_Commit(frame);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapFrameEncode_Commit' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapEncoder_Commit(encoder);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapEncoder_Commit' failed with HRESULT 0x%X", e);
		goto except;
	}

	HGLOBAL mem = NULL;
	e = GetHGlobalFromStream(stream, &mem);
	if (e != S_OK) {
		MTY_Log("'GetHGlobalFromStream' failed with HRESULT 0x%X", e);
		goto except;
	}

	const void *lmem = GlobalLock(mem);
	if (lmem) {
		*outputSize = GlobalSize(mem);
		cmp = MTY_Dup(lmem, *outputSize);

		GlobalUnlock(mem);

	} else {
		MTY_Log("'GlobalLock' failed with error 0x%X", GetLastError());
		goto except;
	}

	except:

	if (frame)
		IWICBitmapFrameEncode_Release(frame);

	if (stream)
		IStream_Release(stream);

	if (encoder)
		IWICBitmapEncoder_Release(encoder);

	if (factory)
		IWICImagingFactory_Release(factory);

	if (ce == S_FALSE || ce == S_OK)
		CoUninitialize();

	return cmp;
}

void *MTY_DecompressImage(const void *input, size_t size, uint32_t *width, uint32_t *height)
{
	void *rgba = NULL;
	IStream *stream = NULL;
	IWICImagingFactory *factory = NULL;
	IWICBitmapDecoder *decoder = NULL;
	IWICBitmapFrameDecode *frame = NULL;
	IWICBitmapSource *sframe = NULL;
	IWICBitmapSource *cframe = NULL;

	HRESULT ce = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	HRESULT e = CoCreateInstance(&CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, &factory);
	if (e != S_OK) {
		MTY_Log("'CoCreateInstance' failed with HRESULT 0x%X", e);
		goto except;
	}

	stream = SHCreateMemStream(input, (UINT) size);
	if (!stream) {
		MTY_Log("'SHCreateMemStream' failed");
		goto except;
	}

	e = IWICImagingFactory_CreateDecoderFromStream(factory, stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder);
	if (e != S_OK) {
		MTY_Log("'IWICImagingFactory_CreateDecoderFromStream' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapDecoder_GetFrame' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapFrameDecode_GetSize(frame, width, height);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapFrameDecode_GetSize' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IWICBitmapFrameDecode_QueryInterface(frame, &IID_IWICBitmapSource, &sframe);
	if (e != S_OK) {
		MTY_Log("'IWICBitmapFrameDecode_QueryInterface' failed with HRESULT 0x%X", e);
		goto except;
	}

	IWICBitmapFrameDecode_Release(frame);
	frame = NULL;

	e = WICConvertBitmapSource(&GUID_WICPixelFormat32bppRGBA, sframe, &cframe);
	if (e != S_OK) {
		MTY_Log("'WICConvertBitmapSource' failed with HRESULT 0x%X", e);
		goto except;
	}

	IWICBitmapSource_Release(sframe);
	sframe = NULL;

	UINT stride = *width * 4;
	UINT rgba_size = stride * *height;
	rgba = MTY_Alloc(rgba_size, 1);

	e = IWICBitmapSource_CopyPixels(cframe, NULL, stride, rgba_size, rgba);
	if (e != S_OK) {
		MTY_Free(rgba);
		rgba = NULL;

		MTY_Log("'IWICBitmapSource_CopyPixels' failed with HRESULT 0x%X", e);
		goto except;
	}

	except:

	if (cframe)
		IWICBitmapSource_Release(cframe);

	if (sframe)
		IWICBitmapSource_Release(sframe);

	if (frame)
		IWICBitmapFrameDecode_Release(frame);

	if (decoder)
		IWICBitmapDecoder_Release(decoder);

	if (stream)
		IStream_Release(stream);

	if (factory)
		IWICImagingFactory_Release(factory);

	if (ce == S_FALSE || ce == S_OK)
		CoUninitialize();

	return rgba;
}

void MTY_DecompressImageAsync(const void *input, size_t size, MTY_ImageFunc func, void *opaque)
{
	uint32_t w = 0;
	uint32_t h = 0;

	void *image = MTY_DecompressImage(input, size, &w, &h);

	func(image, w, h, opaque);
}


// Program Icons

void *MTY_GetProgramIcon(const char *path, uint32_t *width, uint32_t *height)
{
	uint8_t *buf = NULL;
	uint8_t *bmp = NULL;
	uint8_t *mask = NULL;

	ICONINFO ii = {0};
	BITMAP bmiColor = {0};
	BITMAP bmiMask = {0};

	HICON icon = NULL;
	WCHAR *wpath = MTY_MultiToWideD(path);
	if (ExtractIconEx(wpath, 0, &icon, NULL, 1) != 1)
		goto except;

	if (!GetIconInfo(icon, &ii))
		goto except;

	if (GetObject(ii.hbmColor, sizeof(BITMAP), &bmiColor) == 0)
		goto except;

	if (GetObject(ii.hbmMask, sizeof(BITMAP), &bmiMask) == 0)
		goto except;

	// Color and mask must be same dimensions, 32 bpp color, 1 bpp alpha mask
	if (bmiColor.bmWidth != bmiMask.bmWidth ||
		bmiColor.bmHeight != bmiMask.bmHeight ||
		bmiColor.bmBitsPixel != 32 ||
		bmiMask.bmBitsPixel != 1)
	{
		goto except;
	}

	LONG bitmap_size = bmiColor.bmWidthBytes * bmiColor.bmHeight;
	LONG mask_size = bmiMask.bmWidthBytes * bmiMask.bmHeight;

	bmp = MTY_Alloc(bitmap_size, 1);
	mask = MTY_Alloc(mask_size, 1);

	LONG size = GetBitmapBits(ii.hbmColor, bitmap_size, bmp);
	LONG msize = GetBitmapBits(ii.hbmMask, mask_size, mask);

	if (size != bitmap_size || msize < bitmap_size / 32)
		goto except;

	buf = MTY_Alloc(bmiColor.bmWidth * bmiColor.bmHeight * 4, 1);

	for (LONG y = 0; y < bmiColor.bmHeight; y++) {
		for (LONG x = 0; x < bmiColor.bmWidth; x++) {
			size_t buf_o = y * 4 * bmiColor.bmWidth + x * 4;
			size_t bmp_o = y * bmiColor.bmWidthBytes + x * 4;
			size_t msk_o = y * bmiMask.bmWidthBytes + x / 8;
			uint8_t msk_b = (mask[msk_o] & (1 << (7 - (x % 8)))) ? 0 : 0xFF;

			buf[buf_o + 0] = bmp[bmp_o + 2];
			buf[buf_o + 1] = bmp[bmp_o + 1];
			buf[buf_o + 2] = bmp[bmp_o + 0];
			buf[buf_o + 3] = bmp[bmp_o + 3] != 0 ? bmp[bmp_o + 3] : msk_b;
		}
	}

	*width = bmiColor.bmWidth;
	*height = bmiColor.bmHeight;

	except:

	if (ii.hbmColor)
		DeleteObject(ii.hbmColor);

	if (ii.hbmMask)
		DeleteObject(ii.hbmMask);

	if (icon)
		DestroyIcon(icon);

	MTY_Free(bmp);
	MTY_Free(mask);
	MTY_Free(wpath);

	return buf;
}
