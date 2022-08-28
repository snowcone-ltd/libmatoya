// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#if defined(MTY_VK_WIN32)
	#include <windows.h>
	#include "vulkan/vulkan_win32.h"

	#define VKPROC_SO_NAME "vulkan-1.dll"
	#define VKPROC_SURFACE_EXT "VK_KHR_win32_surface"
	#define VKPROC_FORMAT VK_FORMAT_B8G8R8A8_UNORM
	#define VKPROC_ALPHA VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
	#define VKPROC_OUT_OF_DATE(e) \
		((e) == VK_ERROR_OUT_OF_DATE_KHR || (e) == VK_SUBOPTIMAL_KHR)

#elif defined(MTY_VK_XLIB)
	#include "dl/libx11.h"
	#include "vulkan/vulkan_xlib.h"

	#define VKPROC_SO_NAME "libvulkan.so.1"
	#define VKPROC_SURFACE_EXT "VK_KHR_xlib_surface"
	#define VKPROC_FORMAT VK_FORMAT_B8G8R8A8_UNORM
	#define VKPROC_ALPHA VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
	#define VKPROC_OUT_OF_DATE(e) \
		((e) == VK_ERROR_OUT_OF_DATE_KHR || (e) == VK_SUBOPTIMAL_KHR)

#elif defined(MTY_VK_ANDROID)
	#include <android/native_window.h>
	#include "vulkan/vulkan_android.h"

	#define VKPROC_SO_NAME "libvulkan.so"
	#define VKPROC_SURFACE_EXT "VK_KHR_android_surface"
	#define VKPROC_FORMAT VK_FORMAT_R8G8B8A8_UNORM
	#define VKPROC_ALPHA VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR

	// VK_SUBOPTIMAL_KHR on Android is an orientation change
	#define VKPROC_OUT_OF_DATE(e) \
		((e) == VK_ERROR_OUT_OF_DATE_KHR)
#endif
