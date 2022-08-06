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
	#define LIBX11_NO_FUNCS
	#include "dl/libX11.h"
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

#if !defined(VKPROC_EXTERN)
	#define VKPROC_EXTERN extern
#endif

#define VKPROC_DEF(name) \
	VKPROC_EXTERN PFN_##name name

VKPROC_DEF(vkGetInstanceProcAddr);
VKPROC_DEF(vkCreateInstance);
VKPROC_DEF(vkDestroyInstance);
VKPROC_DEF(vkDestroySurfaceKHR);
VKPROC_DEF(vkEnumeratePhysicalDevices);
VKPROC_DEF(vkGetPhysicalDeviceQueueFamilyProperties);
VKPROC_DEF(vkCreateDevice);
VKPROC_DEF(vkDestroyDevice);
VKPROC_DEF(vkGetPhysicalDeviceSurfaceSupportKHR);
VKPROC_DEF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
VKPROC_DEF(vkCreateSwapchainKHR);
VKPROC_DEF(vkDestroySwapchainKHR);
VKPROC_DEF(vkGetSwapchainImagesKHR);
VKPROC_DEF(vkCreateImageView);
VKPROC_DEF(vkDestroyImageView);
VKPROC_DEF(vkCreatePipelineLayout);
VKPROC_DEF(vkDestroyPipelineLayout);
VKPROC_DEF(vkCreateShaderModule);
VKPROC_DEF(vkDestroyShaderModule);
VKPROC_DEF(vkCreateRenderPass);
VKPROC_DEF(vkDestroyRenderPass);
VKPROC_DEF(vkCreateGraphicsPipelines);
VKPROC_DEF(vkDestroyPipeline);
VKPROC_DEF(vkCreateFramebuffer);
VKPROC_DEF(vkDestroyFramebuffer);
VKPROC_DEF(vkCreateCommandPool);
VKPROC_DEF(vkDestroyCommandPool);
VKPROC_DEF(vkAllocateCommandBuffers);
VKPROC_DEF(vkBeginCommandBuffer);
VKPROC_DEF(vkResetCommandBuffer);
VKPROC_DEF(vkCmdBeginRenderPass);
VKPROC_DEF(vkCmdBindPipeline);
VKPROC_DEF(vkCmdPushConstants);
VKPROC_DEF(vkCmdEndRenderPass);
VKPROC_DEF(vkEndCommandBuffer);
VKPROC_DEF(vkGetDeviceQueue);
VKPROC_DEF(vkAcquireNextImageKHR);
VKPROC_DEF(vkQueueSubmit);
VKPROC_DEF(vkQueueWaitIdle);
VKPROC_DEF(vkQueuePresentKHR);
VKPROC_DEF(vkCreateBuffer);
VKPROC_DEF(vkDestroyBuffer);
VKPROC_DEF(vkGetBufferMemoryRequirements);
VKPROC_DEF(vkGetPhysicalDeviceMemoryProperties);
VKPROC_DEF(vkAllocateMemory);
VKPROC_DEF(vkFreeMemory);
VKPROC_DEF(vkBindBufferMemory);
VKPROC_DEF(vkMapMemory);
VKPROC_DEF(vkUnmapMemory);
VKPROC_DEF(vkCmdBindVertexBuffers);
VKPROC_DEF(vkCmdCopyBuffer);
VKPROC_DEF(vkFreeCommandBuffers);
VKPROC_DEF(vkCmdBindIndexBuffer);
VKPROC_DEF(vkCmdDrawIndexed);
VKPROC_DEF(vkCreateImage);
VKPROC_DEF(vkDestroyImage);
VKPROC_DEF(vkCreateDescriptorSetLayout);
VKPROC_DEF(vkDestroyDescriptorSetLayout);
VKPROC_DEF(vkCreateDescriptorPool);
VKPROC_DEF(vkDestroyDescriptorPool);
VKPROC_DEF(vkAllocateDescriptorSets);
VKPROC_DEF(vkUpdateDescriptorSets);
VKPROC_DEF(vkCmdBindDescriptorSets);
VKPROC_DEF(vkGetImageMemoryRequirements);
VKPROC_DEF(vkBindImageMemory);
VKPROC_DEF(vkCmdPipelineBarrier);
VKPROC_DEF(vkCmdCopyBufferToImage);
VKPROC_DEF(vkCreateSampler);
VKPROC_DEF(vkDestroySampler);
VKPROC_DEF(vkFreeDescriptorSets);
VKPROC_DEF(vkFlushMappedMemoryRanges);
VKPROC_DEF(vkCmdSetViewport);
VKPROC_DEF(vkCmdSetScissor);
VKPROC_DEF(vkCreateSemaphore);
VKPROC_DEF(vkDestroySemaphore);

VKPROC_DEF(vkCreateDebugUtilsMessengerEXT);
VKPROC_DEF(vkDestroyDebugUtilsMessengerEXT);

#if defined(MTY_VK_WIN32)
	VKPROC_DEF(vkCreateWin32SurfaceKHR);
#elif defined(MTY_VK_XLIB)
	VKPROC_DEF(vkCreateXlibSurfaceKHR);
#elif defined(MTY_VK_ANDROID)
	VKPROC_DEF(vkCreateAndroidSurfaceKHR);
#endif

void mty_vkproc_cleanup_api(void);
bool mty_vkproc_init_api(void);
bool mty_vkproc_init_debug_ext(VkInstance instance);
