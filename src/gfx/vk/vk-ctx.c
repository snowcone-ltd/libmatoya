// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ctx.h"
GFX_CTX_PROTOTYPES(_vk_)

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "vkproc.c"

#define VK_CTX_ENUM_MAX 32

struct vk_swapchain {
	uint32_t nimages;
	VkSwapchainKHR swapchain;
	VkImage images[VK_CTX_ENUM_MAX];
	VkImageView views[VK_CTX_ENUM_MAX];
	VkFramebuffer fbs[VK_CTX_ENUM_MAX];
	bool used[VK_CTX_ENUM_MAX];
	uint32_t w;
	uint32_t h;

	uint32_t bbi;
	VkFramebuffer bb;
};

struct vk_ctx {
	bool vsync;
	MTY_Renderer *renderer;
	MTY_VkDeviceObjects dobjs;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debug;
	VkSurfaceKHR surface;
	VkPhysicalDevice pdevice;
	VkPhysicalDeviceMemoryProperties pdprops;
	VkSemaphore semaphores[2];
	VkDevice device;
	VkRenderPass rp;
	VkCommandPool pool;
	VkCommandBuffer cmd;
	VkQueue q;

	struct vk_swapchain sc;
};


// Device

static bool vk_ctx_get_pdevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice *pdevice, uint32_t *qi)
{
	uint32_t count = VK_CTX_ENUM_MAX;
	VkPhysicalDevice pdevices[VK_CTX_ENUM_MAX];

	VkResult e = vkEnumeratePhysicalDevices(instance, &count, pdevices);
	if (e != VK_SUCCESS)
		return false;

	if (count == 0)
		return false;

	for (uint32_t x = 0; x < count; x++) {
		uint32_t qfcount = VK_CTX_ENUM_MAX;
		VkQueueFamilyProperties qfprops[VK_CTX_ENUM_MAX];

		vkGetPhysicalDeviceQueueFamilyProperties(pdevices[x], &qfcount, qfprops);

		for (uint32_t y = 0; y < qfcount; y++) {
			VkBool32 surface_support = VK_FALSE;
			e = vkGetPhysicalDeviceSurfaceSupportKHR(pdevices[x], y, surface, &surface_support);

			if ((e == VK_SUCCESS && surface_support) && (qfprops[y].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
				*qi = y;
				*pdevice = pdevices[x];
				return true;
			}
		}
	}

	return false;
}

static bool vk_ctx_get_ldevice(VkPhysicalDevice pdevice, uint32_t qi, VkDevice *device)
{
	VkDeviceCreateInfo ci = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = (const char * []) {
			"VK_KHR_swapchain",
		},
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = qi,
			.queueCount = 1,
			.pQueuePriorities = (float []) {
				1.0f
			},
		},
	};

	VkResult e = vkCreateDevice(pdevice, &ci, NULL, device);
	if (e != VK_SUCCESS)
		return false;

	return true;
}


// Swapchain

static void vk_ctx_destroy_swapchain(VkDevice device, struct vk_swapchain *sc)
{
	for (uint32_t x = 0; x < sc->nimages; x++) {
		if (sc->fbs[x])
			vkDestroyFramebuffer(device, sc->fbs[x], NULL);

		if (sc->views[x])
			vkDestroyImageView(device, sc->views[x], NULL);
	}

	if (sc->swapchain)
		vkDestroySwapchainKHR(device, sc->swapchain, NULL);

	memset(sc, 0, sizeof(struct vk_swapchain));
}

static bool vk_ctx_get_surface_size(VkPhysicalDevice pdevice, VkSurfaceKHR surface, uint32_t *w, uint32_t *h)
{
	VkSurfaceCapabilitiesKHR caps = {0};
	VkResult e = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice, surface, &caps);
	if (e != VK_SUCCESS)
		return false;

	*w = caps.currentExtent.width;
	*h = caps.currentExtent.height;

	return true;
}

static bool vk_ctx_create_swapchain(VkSurfaceKHR surface, VkPhysicalDevice pdevice, VkDevice device,
	VkRenderPass rp, bool vsync, struct vk_swapchain *sc)
{
	bool r = true;

	// Surface size
	r = vk_ctx_get_surface_size(pdevice, surface, &sc->w, &sc->h);
	if (!r)
		goto except;

	// Swapchain
	VkSwapchainCreateInfoKHR sci = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.imageFormat = VKPROC_FORMAT,
		.imageExtent = {
			.width = sc->w,
			.height = sc->h
		},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.minImageCount = 3,
		.surface = surface,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VKPROC_ALPHA,
		.presentMode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR,
		.clipped = VK_TRUE,
	};

	VkResult e = vkCreateSwapchainKHR(device, &sci, NULL, &sc->swapchain);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Swapchain image views
	sc->nimages = VK_CTX_ENUM_MAX;
	e = vkGetSwapchainImagesKHR(device, sc->swapchain, &sc->nimages, sc->images);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	for (uint32_t x = 0; x < sc->nimages; x++) {
		VkImageViewCreateInfo ivci = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = sc->images[x],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = sci.imageFormat,
			.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.levelCount = 1,
			.subresourceRange.layerCount = 1,
		};

		e = vkCreateImageView(device, &ivci, NULL, &sc->views[x]);
		if (e != VK_SUCCESS) {
			r = false;
			goto except;
		}

		VkFramebufferCreateInfo fci = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = rp,
			.attachmentCount = 1,
			.pAttachments = &sc->views[x],
			.width = sc->w,
			.height = sc->h,
			.layers = 1,
		};

		// On 32-bit systems, VkFramebuffer can not be larger than SIZE_MAX
		// because it is cast to a pointer
		e = vkCreateFramebuffer(device, &fci, NULL, &sc->fbs[x]);
		if (e != VK_SUCCESS || (uint64_t) sc->fbs[x] > SIZE_MAX) {
			r = false;
			goto except;
		}
	}

	except:

	if (!r)
		vk_ctx_destroy_swapchain(device, sc);

	return r;
}

static void vk_ctx_swapchain_barrier(struct vk_swapchain *sc, VkCommandBuffer cmd, VkImageLayout from, VkImageLayout to)
{
	VkImageMemoryBarrier b = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout = from,
		.newLayout = to,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = sc->images[sc->bbi],
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.levelCount = 1,
		.subresourceRange.layerCount = 1,
	};

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b);
}


// Main

#if defined(MTY_VK_ANDROID)
static bool vk_ctx_refresh_surface(struct vk_ctx *ctx, ANativeWindow *window)
{
	if (ctx->surface) {
		vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
		ctx->surface = VK_NULL_HANDLE;
	}

	VkAndroidSurfaceCreateInfoKHR ci = {
		.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
		.window = window,
	};

	VkResult e = vkCreateAndroidSurfaceKHR(ctx->instance, &ci, NULL, &ctx->surface);

	return e == VK_SUCCESS;
}
#endif

static bool vk_ctx_surface_size_changed(struct vk_ctx *ctx)
{
	struct vk_swapchain *sc = &ctx->sc;

	uint32_t w = sc->w;
	uint32_t h = sc->h;

	if (!vk_ctx_get_surface_size(ctx->pdevice, ctx->surface, &w, &h))
		return false;

	return w != sc->w || h != sc->h;
}

static bool vk_ctx_refresh_swapchain(struct vk_ctx *ctx)
{
	#if defined(MTY_VK_ANDROID)
		ANativeWindow *window = mty_gfx_wait_for_window(10000);
		if (!window)
			return false;
	#endif

	struct vk_swapchain *sc = &ctx->sc;

	vk_ctx_destroy_swapchain(ctx->device, sc);

	#if defined(MTY_VK_ANDROID)
		if (!vk_ctx_refresh_surface(ctx, window))
			return false;
	#endif

	if (!vk_ctx_create_swapchain(ctx->surface, ctx->pdevice, ctx->device, ctx->rp, ctx->vsync, sc))
		return false;

	#if defined(MTY_VK_ANDROID)
		mty_gfx_done_with_window();
	#endif

	return true;
}

#if defined(MTY_VK_DEBUG)
VKPROC_DEF(vkCreateDebugUtilsMessengerEXT);
VKPROC_DEF(vkDestroyDebugUtilsMessengerEXT);

static VkBool32 VKAPI_PTR vk_ctx_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData)
{
	printf("%s\n", pCallbackData->pMessage);

	return VK_FALSE;
}
#endif

struct gfx_ctx *mty_vk_ctx_create(void *native_window, bool vsync)
{
	struct vk_ctx *ctx = MTY_Alloc(1, sizeof(struct vk_ctx));
	ctx->renderer = MTY_RendererCreate();
	ctx->vsync = vsync;

	bool r = true;

	if (!vkproc_global_init()) {
		r = false;
		goto except;
	}

	// Instance
	const char *exts[] = {
		"VK_KHR_surface",
		VKPROC_SURFACE_EXT,

		#if defined(MTY_VK_DEBUG)
		"VK_EXT_debug_utils",
		#endif
	};

	VkInstanceCreateInfo ici = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.enabledExtensionCount = sizeof(exts) / sizeof(const char *),
		.ppEnabledExtensionNames = exts,
		#if defined(MTY_VK_DEBUG)
		.enabledLayerCount = 1,
		.ppEnabledLayerNames = (const char * []) {
			"VK_LAYER_KHRONOS_validation",
		},
		#endif
	};

	VkResult e = vkCreateInstance(&ici, NULL, &ctx->instance);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Debug
	#if defined(MTY_VK_DEBUG)
	VKPROC_LOAD_SYM_INST(ctx->instance, vkCreateDebugUtilsMessengerEXT);
	VKPROC_LOAD_SYM_INST(ctx->instance, vkDestroyDebugUtilsMessengerEXT);

	VkDebugUtilsMessengerCreateInfoEXT dci = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = vk_ctx_debug_callback,
	};

	e = vkCreateDebugUtilsMessengerEXT(ctx->instance, &dci, NULL, &ctx->debug);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}
	#endif

	// Surface
	#if defined(MTY_VK_WIN32)
	VkWin32SurfaceCreateInfoKHR ci = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = (HINSTANCE) GetWindowLongPtr((HWND) native_window, GWLP_HINSTANCE),
		.hwnd = (HWND) native_window,
	};

	e = vkCreateWin32SurfaceKHR(ctx->instance, &ci, NULL, &ctx->surface);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	#elif defined(MTY_VK_XLIB)
	struct xinfo *info = (struct xinfo *) native_window;

	VkXlibSurfaceCreateInfoKHR ci = {
		.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.dpy = info->display,
		.window = info->window,
	};

	e = vkCreateXlibSurfaceKHR(ctx->instance, &ci, NULL, &ctx->surface);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	#elif defined(MTY_VK_ANDROID)
	ANativeWindow *window = mty_gfx_wait_for_window(10000);

	r = vk_ctx_refresh_surface(ctx, window);

	mty_gfx_done_with_window();

	if (!r)
		goto except;
	#endif

	// Physical device
	uint32_t qi = 0;

	if (!vk_ctx_get_pdevice(ctx->instance, ctx->surface, &ctx->pdevice, &qi)) {
		r = false;
		goto except;
	}

	vkGetPhysicalDeviceMemoryProperties(ctx->pdevice, &ctx->pdprops);

	// Logical device
	if (!vk_ctx_get_ldevice(ctx->pdevice, qi, &ctx->device)) {
		r = false;
		goto except;
	}

	vkGetDeviceQueue(ctx->device, qi, 0, &ctx->q);

	// Create command pool
	VkCommandPoolCreateInfo pci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = qi,
	};

	e = vkCreateCommandPool(ctx->device, &pci, NULL, &ctx->pool);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Command buffer
	VkCommandBufferAllocateInfo cai = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = ctx->pool,
		.commandBufferCount = 1,
	};

	e = vkAllocateCommandBuffers(ctx->device, &cai, &ctx->cmd);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Semaphores
	VkSemaphoreCreateInfo si = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	e = vkCreateSemaphore(ctx->device, &si, NULL, &ctx->semaphores[0]);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	e = vkCreateSemaphore(ctx->device, &si, NULL, &ctx->semaphores[1]);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Render pass
	VkRenderPassCreateInfo rpci = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &(VkAttachmentDescription) {
			.format = VKPROC_FORMAT,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		},
		.subpassCount = 1,
		.pSubpasses = &(VkSubpassDescription) {
			.colorAttachmentCount = 1,
			.pColorAttachments = &(VkAttachmentReference) {
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
		},
	};

	e = vkCreateRenderPass(ctx->device, &rpci, NULL, &ctx->rp);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Swapchain
	r = vk_ctx_create_swapchain(ctx->surface, ctx->pdevice, ctx->device, ctx->rp, ctx->vsync, &ctx->sc);
	if (!r)
		goto except;

	except:

	if (!r)
		mty_vk_ctx_destroy((struct gfx_ctx **) &ctx);

	return (struct gfx_ctx *) ctx;
}

void mty_vk_ctx_destroy(struct gfx_ctx **gfx_ctx)
{
	if (!gfx_ctx || !*gfx_ctx)
		return;

	struct vk_ctx *ctx = (struct vk_ctx *) *gfx_ctx;

	MTY_RendererDestroy(&ctx->renderer);

	if (ctx->instance) {
		if (ctx->device) {
			vk_ctx_destroy_swapchain(ctx->device, &ctx->sc);

			if (ctx->rp)
				vkDestroyRenderPass(ctx->device, ctx->rp, NULL);

			for (uint8_t x = 0; x < 2; x++)
				if (ctx->semaphores[x])
					vkDestroySemaphore(ctx->device, ctx->semaphores[x], NULL);

			if (ctx->pool) {
				if (ctx->cmd)
					vkFreeCommandBuffers(ctx->device, ctx->pool, 1, &ctx->cmd);

				vkDestroyCommandPool(ctx->device, ctx->pool, NULL);
			}

			vkDestroyDevice(ctx->device, NULL);
		}

		if (ctx->surface)
			vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);

		#if defined(MTY_VK_DEBUG)
		if (ctx->debug)
			vkDestroyDebugUtilsMessengerEXT(ctx->instance, ctx->debug, NULL);
		#endif

		vkDestroyInstance(ctx->instance, NULL);
	}

	vkproc_global_destroy();

	MTY_Free(ctx);
	*gfx_ctx = NULL;
}

MTY_Device *mty_vk_ctx_get_device(struct gfx_ctx *gfx_ctx)
{
	struct vk_ctx *ctx = (struct vk_ctx *) gfx_ctx;

	ctx->dobjs.device = ctx->device;
	ctx->dobjs.physicalDeviceMemoryProperties = &ctx->pdprops;

	return (MTY_Device *) &ctx->dobjs;
}

MTY_Context *mty_vk_ctx_get_context(struct gfx_ctx *gfx_ctx)
{
	struct vk_ctx *ctx = (struct vk_ctx *) gfx_ctx;

	return (MTY_Context *) ctx->cmd;
}

MTY_Surface *mty_vk_ctx_get_surface(struct gfx_ctx *gfx_ctx)
{
	struct vk_ctx *ctx = (struct vk_ctx *) gfx_ctx;

	struct vk_swapchain *sc = &ctx->sc;

	if (!sc->bb || !sc->swapchain) {
		// Recreate swapchain on any surface size change
		if (vk_ctx_surface_size_changed(ctx))
			vk_ctx_refresh_swapchain(ctx);

		// Get next swapchain buffer
		VkResult e = vkAcquireNextImageKHR(ctx->device, sc->swapchain, UINT64_MAX,
			ctx->semaphores[0], VK_NULL_HANDLE, &sc->bbi);

		// Recreate swapchain if necessary
		if (VKPROC_OUT_OF_DATE(e)) {
			vk_ctx_refresh_swapchain(ctx);
			sc->bb = VK_NULL_HANDLE;
			return NULL;
		}

		// Reset command buffer -- doing it this way may reuse resources
		vkResetCommandBuffer(ctx->cmd, 0);

		// Begin command buffer
		VkCommandBufferBeginInfo cbi = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};

		e = vkBeginCommandBuffer(ctx->cmd, &cbi);
		if (e != VK_SUCCESS)
			return NULL;

		// Images start in VK_IMAGE_LAYOUT_UNDEFINED, after first present they will be
		// left in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		VkImageLayout begin = sc->used[sc->bbi] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
		vk_ctx_swapchain_barrier(sc, ctx->cmd, begin, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		sc->used[sc->bbi] = true;
		sc->bb = sc->fbs[sc->bbi];
	}

	return (MTY_Surface *) (uintptr_t) sc->bb;
}

void mty_vk_ctx_present(struct gfx_ctx *gfx_ctx)
{
	struct vk_ctx *ctx = (struct vk_ctx *) gfx_ctx;

	struct vk_swapchain *sc = &ctx->sc;

	if (!sc->bb)
		return;

	// Present barrier
	vk_ctx_swapchain_barrier(sc, ctx->cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	// End command buffer
	vkEndCommandBuffer(ctx->cmd);

	// Submit command buffer to graphics queue
	VkSubmitInfo si = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ctx->semaphores[0],
		.pWaitDstStageMask = (VkPipelineStageFlags []) {
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		},
		.commandBufferCount = 1,
		.pCommandBuffers = &ctx->cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &ctx->semaphores[1],
	};

	// Submit commands
	vkQueueSubmit(ctx->q, 1, &si, VK_NULL_HANDLE);

	// Present
	VkPresentInfoKHR pi = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ctx->semaphores[1],
		.swapchainCount = 1,
		.pSwapchains = &sc->swapchain,
		.pImageIndices = &sc->bbi,
	};

	VkResult e = vkQueuePresentKHR(ctx->q, &pi);

	vkQueueWaitIdle(ctx->q);

	// Recreate swapchain if necessary
	if (VKPROC_OUT_OF_DATE(e))
		vk_ctx_refresh_swapchain(ctx);

	sc->bb = VK_NULL_HANDLE;
}

void mty_vk_ctx_draw_quad(struct gfx_ctx *gfx_ctx, const void *image, const MTY_RenderDesc *desc)
{
	struct vk_ctx *ctx = (struct vk_ctx *) gfx_ctx;

	struct vk_swapchain *sc = &ctx->sc;

	mty_vk_ctx_get_surface(gfx_ctx);

	if (sc->bb) {
		MTY_RenderDesc mutated = *desc;
		mutated.viewWidth = sc->w;
		mutated.viewHeight = sc->h;

		#if defined(MTY_VK_ANDROID)
			mutated.viewHeight -= mty_app_get_kb_height();
		#endif

		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_VK, mty_vk_ctx_get_device(gfx_ctx),
			mty_vk_ctx_get_context(gfx_ctx), image, &mutated, (MTY_Surface *) (uintptr_t) sc->bb);
	}
}

void mty_vk_ctx_draw_ui(struct gfx_ctx *gfx_ctx, const MTY_DrawData *dd)
{
	struct vk_ctx *ctx = (struct vk_ctx *) gfx_ctx;

	struct vk_swapchain *sc = &ctx->sc;

	mty_vk_ctx_get_surface(gfx_ctx);

	if (sc->bb) {
		MTY_DrawData mutated = *dd;
		mutated.displaySize.x = (float) sc->w;
		mutated.displaySize.y = (float) sc->h;

		MTY_RendererDrawUI(ctx->renderer, MTY_GFX_VK, mty_vk_ctx_get_device(gfx_ctx),
			mty_vk_ctx_get_context(gfx_ctx), &mutated, (MTY_Surface *) (uintptr_t) sc->bb);
	}
}

bool mty_vk_ctx_set_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct vk_ctx *ctx = (struct vk_ctx *) gfx_ctx;

	return MTY_RendererSetUITexture(ctx->renderer, MTY_GFX_VK, mty_vk_ctx_get_device(gfx_ctx),
		mty_vk_ctx_get_context(gfx_ctx), id, rgba, width, height);
}

bool mty_vk_ctx_has_ui_texture(struct gfx_ctx *gfx_ctx, uint32_t id)
{
	struct vk_ctx *ctx = (struct vk_ctx *) gfx_ctx;

	return MTY_RendererHasUITexture(ctx->renderer, id);
}
