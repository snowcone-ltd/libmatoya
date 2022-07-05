// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#define VKPROC_EXTERN
#include "vkproc.h"

#include <string.h>

#define VKPROC_LOAD_SYM(name) \
	name = (PFN_##name) MTY_SOGetSymbol(VKPROC_SO, #name); \
	if (!name) {r = false; goto except;}

#define VKPROC_LOAD_SYM_INST(instance, name) \
	name = (PFN_##name) vkGetInstanceProcAddr(instance, #name); \
	if (!name) {r = false; goto except;}

static MTY_SO *VKPROC_SO;
static MTY_Atomic32 VKPROC_LOCK;
static uint32_t VKPROC_REF;

void mty_vkproc_cleanup_api(void)
{
	MTY_GlobalLock(&VKPROC_LOCK);

	if (VKPROC_REF > 0)
		if (--VKPROC_REF == 0)
			MTY_SOUnload(&VKPROC_SO);

	MTY_GlobalUnlock(&VKPROC_LOCK);
}

bool mty_vkproc_init_api(void)
{
	bool r = true;

	MTY_GlobalLock(&VKPROC_LOCK);

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
	}

	except:

	if (r) {
		VKPROC_REF++;

	} else {
		MTY_SOUnload(&VKPROC_SO);
	}

	MTY_GlobalUnlock(&VKPROC_LOCK);

	return r;
}

bool mty_vkproc_init_debug_ext(VkInstance instance)
{
	bool r = true;

	VKPROC_LOAD_SYM_INST(instance, vkCreateDebugUtilsMessengerEXT);
	VKPROC_LOAD_SYM_INST(instance, vkDestroyDebugUtilsMessengerEXT);

	except:

	return r;
}
