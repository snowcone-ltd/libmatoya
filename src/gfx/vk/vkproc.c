// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"
#include "vkproc.h"

#define VKPROC_LOAD_SYM(name) \
	name = (PFN_##name) MTY_SOGetSymbol(VKPROC_SO, #name); \
	if (!name) {r = false; goto except;}

#define VKPROC_LOAD_SYM_INST(instance, name) \
	name = (PFN_##name) vkGetInstanceProcAddr(instance, #name); \
	if (!name) {r = false; goto except;}

#define VKPROC_DEF(name) \
	static PFN_##name name

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

#if defined(MTY_VK_WIN32)
	VKPROC_DEF(vkCreateWin32SurfaceKHR);
#elif defined(MTY_VK_XLIB)
	VKPROC_DEF(vkCreateXlibSurfaceKHR);
#elif defined(MTY_VK_ANDROID)
	VKPROC_DEF(vkCreateAndroidSurfaceKHR);
#endif

static MTY_SO *VKPROC_SO;
static MTY_Atomic32 VKPROC_LOCK;
static uint32_t VKPROC_REF;

static void vkproc_global_destroy(void)
{
	MTY_GlobalLock(&VKPROC_LOCK);

	if (VKPROC_REF > 0)
		if (--VKPROC_REF == 0)
			MTY_SOUnload(&VKPROC_SO);

	MTY_GlobalUnlock(&VKPROC_LOCK);
}

static bool vkproc_global_init(void)
{
	MTY_GlobalLock(&VKPROC_LOCK);

	bool r = true;

	if (VKPROC_REF == 0) {
		VKPROC_SO = MTY_SOLoad(VKPROC_SO_NAME);
		if (!VKPROC_SO) {
			r = false;
			goto except;
		}

		VKPROC_LOAD_SYM(vkGetInstanceProcAddr);
		VKPROC_LOAD_SYM(vkCreateInstance);
		VKPROC_LOAD_SYM(vkDestroyInstance);
		VKPROC_LOAD_SYM(vkDestroySurfaceKHR);
		VKPROC_LOAD_SYM(vkEnumeratePhysicalDevices);
		VKPROC_LOAD_SYM(vkGetPhysicalDeviceQueueFamilyProperties);
		VKPROC_LOAD_SYM(vkCreateDevice);
		VKPROC_LOAD_SYM(vkDestroyDevice);
		VKPROC_LOAD_SYM(vkGetPhysicalDeviceSurfaceSupportKHR);
		VKPROC_LOAD_SYM(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
		VKPROC_LOAD_SYM(vkCreateSwapchainKHR);
		VKPROC_LOAD_SYM(vkDestroySwapchainKHR);
		VKPROC_LOAD_SYM(vkGetSwapchainImagesKHR);
		VKPROC_LOAD_SYM(vkCreateImageView);
		VKPROC_LOAD_SYM(vkDestroyImageView);
		VKPROC_LOAD_SYM(vkCreatePipelineLayout);
		VKPROC_LOAD_SYM(vkDestroyPipelineLayout);
		VKPROC_LOAD_SYM(vkCreateShaderModule);
		VKPROC_LOAD_SYM(vkDestroyShaderModule);
		VKPROC_LOAD_SYM(vkCreateRenderPass);
		VKPROC_LOAD_SYM(vkDestroyRenderPass);
		VKPROC_LOAD_SYM(vkCreateGraphicsPipelines);
		VKPROC_LOAD_SYM(vkDestroyPipeline);
		VKPROC_LOAD_SYM(vkCreateFramebuffer);
		VKPROC_LOAD_SYM(vkDestroyFramebuffer);
		VKPROC_LOAD_SYM(vkCreateCommandPool);
		VKPROC_LOAD_SYM(vkDestroyCommandPool);
		VKPROC_LOAD_SYM(vkAllocateCommandBuffers);
		VKPROC_LOAD_SYM(vkBeginCommandBuffer);
		VKPROC_LOAD_SYM(vkResetCommandBuffer);
		VKPROC_LOAD_SYM(vkCmdBeginRenderPass);
		VKPROC_LOAD_SYM(vkCmdBindPipeline);
		VKPROC_LOAD_SYM(vkCmdPushConstants);
		VKPROC_LOAD_SYM(vkCmdEndRenderPass);
		VKPROC_LOAD_SYM(vkEndCommandBuffer);
		VKPROC_LOAD_SYM(vkGetDeviceQueue);
		VKPROC_LOAD_SYM(vkAcquireNextImageKHR);
		VKPROC_LOAD_SYM(vkQueueSubmit);
		VKPROC_LOAD_SYM(vkQueueWaitIdle);
		VKPROC_LOAD_SYM(vkQueuePresentKHR);
		VKPROC_LOAD_SYM(vkCreateBuffer);
		VKPROC_LOAD_SYM(vkDestroyBuffer);
		VKPROC_LOAD_SYM(vkGetBufferMemoryRequirements);
		VKPROC_LOAD_SYM(vkGetPhysicalDeviceMemoryProperties);
		VKPROC_LOAD_SYM(vkAllocateMemory);
		VKPROC_LOAD_SYM(vkFreeMemory);
		VKPROC_LOAD_SYM(vkBindBufferMemory);
		VKPROC_LOAD_SYM(vkMapMemory);
		VKPROC_LOAD_SYM(vkUnmapMemory);
		VKPROC_LOAD_SYM(vkCmdBindVertexBuffers);
		VKPROC_LOAD_SYM(vkCmdCopyBuffer);
		VKPROC_LOAD_SYM(vkFreeCommandBuffers);
		VKPROC_LOAD_SYM(vkCmdBindIndexBuffer);
		VKPROC_LOAD_SYM(vkCmdDrawIndexed);
		VKPROC_LOAD_SYM(vkCreateImage);
		VKPROC_LOAD_SYM(vkDestroyImage);
		VKPROC_LOAD_SYM(vkCreateDescriptorSetLayout);
		VKPROC_LOAD_SYM(vkDestroyDescriptorSetLayout);
		VKPROC_LOAD_SYM(vkCreateDescriptorPool);
		VKPROC_LOAD_SYM(vkDestroyDescriptorPool);
		VKPROC_LOAD_SYM(vkAllocateDescriptorSets);
		VKPROC_LOAD_SYM(vkUpdateDescriptorSets);
		VKPROC_LOAD_SYM(vkCmdBindDescriptorSets);
		VKPROC_LOAD_SYM(vkGetImageMemoryRequirements);
		VKPROC_LOAD_SYM(vkBindImageMemory);
		VKPROC_LOAD_SYM(vkCmdPipelineBarrier);
		VKPROC_LOAD_SYM(vkCmdCopyBufferToImage);
		VKPROC_LOAD_SYM(vkCreateSampler);
		VKPROC_LOAD_SYM(vkDestroySampler);
		VKPROC_LOAD_SYM(vkFreeDescriptorSets);
		VKPROC_LOAD_SYM(vkFlushMappedMemoryRanges);
		VKPROC_LOAD_SYM(vkCmdSetViewport);
		VKPROC_LOAD_SYM(vkCmdSetScissor);
		VKPROC_LOAD_SYM(vkCreateSemaphore);
		VKPROC_LOAD_SYM(vkDestroySemaphore);

		#if defined(MTY_VK_WIN32)
			VKPROC_LOAD_SYM(vkCreateWin32SurfaceKHR);
		#elif defined(MTY_VK_XLIB)
			VKPROC_LOAD_SYM(vkCreateXlibSurfaceKHR);
		#elif defined(MTY_VK_ANDROID)
			VKPROC_LOAD_SYM(vkCreateAndroidSurfaceKHR);
		#endif

		except:

		if (!r)
			MTY_SOUnload(&VKPROC_SO);
	}

	if (r)
		VKPROC_REF++;

	MTY_GlobalUnlock(&VKPROC_LOCK);

	return r;
}
