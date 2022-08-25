// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <string.h>

#include "vkproc.c"

struct vk_buffer {
	VkBuffer buf;
	VkDeviceMemory mem;
};

struct vk_image {
	struct vk_buffer buf;
	VkDeviceMemory mem;
	VkFormat format;
	VkImage image;
	VkImageView view;
	uint32_t w;
	uint32_t h;
};


// Memory

static bool vk_allocate_memory(VkDevice device, const VkPhysicalDeviceMemoryProperties *pdprops,
	const VkMemoryRequirements *mreq, VkMemoryPropertyFlags want, VkDeviceMemory *mem)
{
	for (uint32_t i = 0; i < pdprops->memoryTypeCount; ++i) {
		bool match = (pdprops->memoryTypes[i].propertyFlags & want) == want;

		if ((mreq->memoryTypeBits & (1 << i)) && match) {
			VkMemoryAllocateInfo ai = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = mreq->size,
				.memoryTypeIndex = i,
			};

			VkResult e = vkAllocateMemory(device, &ai, NULL, mem);
			if (e == VK_SUCCESS)
				return true;
		}
	}

	return false;
}

static bool vk_buffer_upload(VkDevice device, VkDeviceMemory dst, const void *src, size_t size)
{
	void *p = NULL;
	VkResult e = vkMapMemory(device, dst, 0, VK_WHOLE_SIZE, 0, &p);
	if (e != VK_SUCCESS)
		return false;

	memcpy(p, src, size);

	VkMappedMemoryRange range = {
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.size = VK_WHOLE_SIZE,
		.memory = dst,
	};

	e = vkFlushMappedMemoryRanges(device, 1, &range);
	if (e != VK_SUCCESS)
		return false;

	vkUnmapMemory(device, dst);

	return true;
}


// Buffer

static void vk_destroy_buffer(VkDevice device, struct vk_buffer *buf)
{
	if (!buf)
		return;

	if (buf->buf)
		vkDestroyBuffer(device, buf->buf, NULL);

	if (buf->mem)
		vkFreeMemory(device, buf->mem, NULL);

	memset(buf, 0, sizeof(struct vk_buffer));
}

static bool vk_allocate_buffer(const VkPhysicalDeviceMemoryProperties *pdprops, VkDevice device,
	VkBufferUsageFlags usage, VkMemoryPropertyFlags mprops, VkDeviceSize size, struct vk_buffer *buf)
{
	bool r = true;

	VkBufferCreateInfo bi = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.usage = usage,
		.size = size,
	};

	VkResult e = vkCreateBuffer(device, &bi, NULL, &buf->buf);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	VkMemoryRequirements mreq = {0};
	vkGetBufferMemoryRequirements(device, buf->buf, &mreq);

	r = vk_allocate_memory(device, pdprops, &mreq, mprops, &buf->mem);
	if (!r)
		goto except;

	e = vkBindBufferMemory(device, buf->buf, buf->mem, 0);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	except:

	if (!r)
		vk_destroy_buffer(device, buf);

	return r;
}


// Resource

static void vk_destroy_image(VkDevice device, struct vk_image *img)
{
	if (!img)
		return;

	if (img->view)
		vkDestroyImageView(device, img->view, NULL);

	if (img->image)
		vkDestroyImage(device, img->image, NULL);

	if (img->mem)
		vkFreeMemory(device, img->mem, NULL);

	vk_destroy_buffer(device, &img->buf);

	memset(img, 0, sizeof(struct vk_image));
}

static bool vk_create_image(const VkPhysicalDeviceMemoryProperties *pdprops, VkDevice device,
	VkFormat format, uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp, struct vk_image *img)
{
	bool r = true;

	VkDeviceSize size = full_w * h * bpp;

	r = vk_allocate_buffer(pdprops, device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, &img->buf);
	if (!r)
		goto except;

	VkImageCreateInfo ii = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.extent.width = w,
		.extent.height = h,
		.extent.depth = 1,
		.mipLevels = 1,
		.arrayLayers = 1,
		.format = format,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.samples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkResult e = vkCreateImage(device, &ii, NULL, &img->image);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	VkMemoryRequirements mreq = {0};
	vkGetImageMemoryRequirements(device, img->image, &mreq);

	r = vk_allocate_memory(device, pdprops, &mreq,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &img->mem);
	if (!r)
		goto except;

	e = vkBindImageMemory(device, img->image, img->mem, 0);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	VkImageViewCreateInfo ici = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = img->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.levelCount = 1,
		.subresourceRange.layerCount = 1,
	};

	e = vkCreateImageView(device, &ici, NULL, &img->view);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	img->w = w;
	img->h = h;
	img->format = format;

	except:

	if (!r)
		vk_destroy_image(device, img);

	return r;
}

static void vk_image_gpu_copy(struct vk_image *img, VkCommandBuffer cmd, uint32_t full_w, uint32_t w, uint32_t h)
{
	// Write barrier
	VkImageMemoryBarrier b = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = img->image,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.levelCount = 1,
		.subresourceRange.layerCount = 1,
	};

	VkPipelineStageFlags ss = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags ds = VK_PIPELINE_STAGE_TRANSFER_BIT;
	vkCmdPipelineBarrier(cmd, ss, ds, 0, 0, NULL, 0, NULL, 1, &b);

	// GPU transfer
	VkBufferImageCopy rg = {
		.bufferRowLength = full_w,
		.bufferImageHeight = h,
		.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.imageSubresource.layerCount = 1,
		.imageExtent.width = w,
		.imageExtent.height = h,
		.imageExtent.depth = 1,
	};

	vkCmdCopyBufferToImage(cmd, img->buf.buf, img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &rg);

	// Read barrier
	b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	ss = VK_PIPELINE_STAGE_TRANSFER_BIT;
	ds = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	vkCmdPipelineBarrier(cmd, ss, ds, 0, 0, NULL, 0, NULL, 1, &b);
}
