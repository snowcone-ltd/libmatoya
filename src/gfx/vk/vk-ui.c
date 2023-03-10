// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ui.h"
GFX_UI_PROTOTYPES(_vk_)

#include <math.h>

#include "vk-common.h"

static
#include "shaders/fsui.h"

static
#include "shaders/vsui.h"

#define VK_UI_SETS_MAX 1024

struct vk_ui_buffer {
	struct vk_buffer buf;
	uint32_t len;
	void *sys;
};

struct vk_ui_image {
	struct vk_image img;
	VkDescriptorSet dset;
	uint32_t w;
	uint32_t h;
	bool transfer;
};

struct vk_ui {
	VkShaderModule vert;
	VkShaderModule frag;
	VkRenderPass rp;
	VkRenderPass rp_clear;
	VkSampler sampler;
	VkDescriptorPool dpool;
	VkDescriptorSetLayout dlayout;
	VkPipelineLayout layout;
	VkPipeline pipeline;

	struct vk_ui_image *clear_img;
	struct vk_ui_buffer vb;
	struct vk_ui_buffer ib;
};

struct gfx_ui *mty_vk_ui_create(struct gfx_device *device)
{
	struct vk_ui *ctx = MTY_Alloc(1, sizeof(struct vk_ui));

	bool r = true;

	struct vk_device_objects *dobjs = (struct vk_device_objects *) device;

	VkDevice _device = dobjs->device;

	if (!vkproc_global_init()) {
		r = false;
		goto except;
	}

	// Vertex shader
	VkShaderModuleCreateInfo shci = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = sizeof(VERT),
		.pCode = VERT,
	};

	VkResult e = vkCreateShaderModule(_device, &shci, NULL, &ctx->vert);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Fragment shader
	shci.codeSize = sizeof(FRAG);
	shci.pCode = FRAG;

	e = vkCreateShaderModule(_device, &shci, NULL, &ctx->frag);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Render passes
	VkAttachmentDescription ad = {
		.format = VKPROC_FORMAT,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkRenderPassCreateInfo rpci = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &ad,
		.subpassCount = 1,
		.pSubpasses = &(VkSubpassDescription) {
			.colorAttachmentCount = 1,
			.pColorAttachments = &(VkAttachmentReference) {
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
		},
	};

	e = vkCreateRenderPass(_device, &rpci, NULL, &ctx->rp);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

	e = vkCreateRenderPass(_device, &rpci, NULL, &ctx->rp_clear);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Linear sampler
	VkSamplerCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	};

	e = vkCreateSampler(_device, &sci, NULL, &ctx->sampler);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Descriptor pool
	VkDescriptorPoolCreateInfo dpi = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.poolSizeCount = 1,
		.pPoolSizes = (VkDescriptorPoolSize []) {{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = VK_UI_SETS_MAX,
		}},
		.maxSets = VK_UI_SETS_MAX,
	};

	e = vkCreateDescriptorPool(_device, &dpi, NULL, &ctx->dpool);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Descriptor set layout
	VkDescriptorSetLayoutCreateInfo lci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = (VkDescriptorSetLayoutBinding []) {
			{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
		},
	};

	e = vkCreateDescriptorSetLayout(_device, &lci, NULL, &ctx->dlayout);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Pipeline layout
	VkPipelineLayoutCreateInfo pci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &ctx->dlayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &(VkPushConstantRange) {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.size = 4 * sizeof(float),
		},
	};

	e = vkCreatePipelineLayout(_device, &pci, NULL, &ctx->layout);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Pipeline
	VkGraphicsPipelineCreateInfo pinfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = (VkPipelineShaderStageCreateInfo []) {{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = ctx->vert,
			.pName = "main",
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = ctx->frag,
			.pName = "main",
		}},
		.pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
				.stride = sizeof(MTY_Vtx),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			},
			.vertexAttributeDescriptionCount = 3,
			.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription []) {{
				.location = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(MTY_Vtx, pos),
			}, {
				.location = 1,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(MTY_Vtx, uv),
			}, {
				.location = 2,
				.format = VK_FORMAT_R8G8B8A8_UNORM,
				.offset = offsetof(MTY_Vtx, col),
			}},
		},
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		},
		.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.lineWidth = 1.0f,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
		},
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		},
		.pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &(VkPipelineColorBlendAttachmentState) {
				.blendEnable = VK_TRUE,
				.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
					VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			},
		},
		.pViewportState = &(VkPipelineViewportStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
		},
		.pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 2,
			.pDynamicStates = (VkDynamicState []) {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR,
			},
		},
		.layout = ctx->layout,
		.renderPass = ctx->rp,
	};

	e = vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pinfo, NULL, &ctx->pipeline);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Dummy image for empty binding
	void *rgba = MTY_Alloc(256 * 256, 4);
	ctx->clear_img = mty_vk_ui_create_texture((struct gfx_ui *) ctx, device, rgba, 256, 256);
	MTY_Free(rgba);

	if (!ctx->clear_img) {
		r = false;
		goto except;
	}

	except:

	if (!r)
		mty_vk_ui_destroy((struct gfx_ui **) &ctx, device);

	return (struct gfx_ui *) ctx;
}

static void vk_ui_destroy_buffer(VkDevice device, struct vk_ui_buffer *uibuf)
{
	if (!uibuf)
		return;

	vk_destroy_buffer(device, &uibuf->buf);

	MTY_Free(uibuf->sys);

	memset(uibuf, 0, sizeof(struct vk_ui_buffer));
}

static bool vk_ui_resize_buffer(struct vk_ui_buffer *uibuf, const VkPhysicalDeviceMemoryProperties *pdprops,
	VkDevice device, VkBufferUsageFlagBits usage, uint32_t len, uint32_t incr, uint32_t element_size)
{
	if (uibuf->len < len) {
		vk_destroy_buffer(device, &uibuf->buf);

		uibuf->len = len + incr;
		uibuf->sys = MTY_Realloc(uibuf->sys, uibuf->len, element_size);

		return vk_allocate_buffer(pdprops, device, usage,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, uibuf->len * element_size, &uibuf->buf);
	}

	return true;
}

bool mty_vk_ui_render(struct gfx_ui *gfx_ui, struct gfx_device *device, struct gfx_context *context,
	const MTY_DrawData *dd, MTY_Hash *cache, struct gfx_surface *dest)
{
	struct vk_ui *ctx = (struct vk_ui *) gfx_ui;

	struct vk_device_objects *dobjs = (struct vk_device_objects *) device;

	const VkPhysicalDeviceMemoryProperties *pdprops = dobjs->physicalDeviceMemoryProperties;
	VkDevice _device = dobjs->device;

	VkCommandBuffer cmd = (VkCommandBuffer) context;

	VkFramebuffer fb = (VkFramebuffer) dest;
	if (!fb)
		return false;

	// Prevent rendering under invalid scenarios
	if (dd->displaySize.x <= 0 || dd->displaySize.y <= 0 || dd->cmdListLength == 0)
		return false;

	// Resize vertex/index buffers and upload
	bool r = vk_ui_resize_buffer(&ctx->vb, pdprops, _device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		dd->vtxTotalLength, GFX_UI_VTX_INCR, sizeof(MTY_Vtx));
	if (!r)
		return false;

	r = vk_ui_resize_buffer(&ctx->ib, pdprops, _device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		dd->idxTotalLength, GFX_UI_IDX_INCR, sizeof(uint16_t));
	if (!r)
		return false;

	MTY_Vtx *vtx_dst = ctx->vb.sys;
	uint16_t *idx_dst = ctx->ib.sys;

	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		MTY_CmdList *cmdList = &dd->cmdList[x];

		memcpy(vtx_dst, cmdList->vtx, cmdList->vtxLength * sizeof(MTY_Vtx));
		memcpy(idx_dst, cmdList->idx, cmdList->idxLength * sizeof(uint16_t));

		vtx_dst += cmdList->vtxLength;
		idx_dst += cmdList->idxLength;
	}

	r = vk_buffer_upload(_device, ctx->vb.buf.mem, ctx->vb.sys, dd->vtxTotalLength * sizeof(MTY_Vtx));
	if (!r)
		return false;

	r = vk_buffer_upload(_device, ctx->ib.buf.mem, ctx->ib.sys, dd->idxTotalLength * sizeof(uint16_t));
	if (!r)
		return false;

	// Transfer textures to GPU -- must happend before render pass begins
	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		MTY_CmdList *cmdList = &dd->cmdList[x];

		for (uint32_t y = 0; y < cmdList->cmdLength; y++) {
			MTY_Cmd *pcmd = &cmdList->cmd[y];

			struct vk_ui_image *uiimg = MTY_HashGetInt(cache, pcmd->texture);
			if (!uiimg)
				uiimg = ctx->clear_img;

			if (!uiimg->transfer) {
				vk_image_gpu_copy(&uiimg->img, cmd, uiimg->w, uiimg->w, uiimg->h);
				uiimg->transfer = true;
			}
		}
	}

	// Begin render pass
	VkRenderPassBeginInfo rpi = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = ctx->rp_clear,
		.framebuffer = fb,
		.renderArea.extent = {
			.width = lrint(dd->displaySize.x),
			.height = lrint(dd->displaySize.y),
		},
		.clearValueCount = 1,
		.pClearValues = &(VkClearValue) {
			.color.float32 = {0.0f, 0.0f, 0.0f, 1.0f}
		},
	};

	if (!dd->clear) {
		rpi.renderPass = ctx->rp;
		rpi.clearValueCount = 0;
		rpi.pClearValues = NULL;
	}

	vkCmdBeginRenderPass(cmd, &rpi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline);

	// Set viewport based on display size
	VkViewport vp = {
		.width = dd->displaySize.x,
		.height = dd->displaySize.y,
		.maxDepth = 1.0f
	};

	vkCmdSetViewport(cmd, 0, 1, &vp);

	// Push projection matrix
	float scale[2] = {
		2.0f / dd->displaySize.x,
		2.0f / dd->displaySize.y,
	};

	float translate[2] = {
		-1.0f,
		-1.0f
	};

	vkCmdPushConstants(cmd, ctx->layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
	vkCmdPushConstants(cmd, ctx->layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);

	// Draw
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &ctx->vb.buf.buf, &offset);
	vkCmdBindIndexBuffer(cmd, ctx->ib.buf.buf, 0, VK_INDEX_TYPE_UINT16);

	uint32_t idxOffset = 0;
	uint32_t vtxOffset = 0;
	struct vk_ui_image *prev_img = NULL;

	for (uint32_t x = 0; x < dd->cmdListLength; x++) {
		MTY_CmdList *cmdList = &dd->cmdList[x];

		for (uint32_t y = 0; y < cmdList->cmdLength; y++) {
			const MTY_Cmd *pcmd = &cmdList->cmd[y];
			const MTY_Rect *c = &pcmd->clip;

			// Make sure the rect is actually in the viewport
			if (c->left < dd->displaySize.x && c->top < dd->displaySize.y && c->right >= 0 && c->bottom >= 0) {

				// Use the clip to apply scissor
				VkRect2D scissor = {
					.offset.x = lrint(c->left),
					.offset.y = lrint(c->top),
					.extent.width = lrint(c->right - c->left),
					.extent.height = lrint(c->bottom - c->top),
				};
				vkCmdSetScissor(cmd, 0, 1, &scissor);

				// Optionally sample from a texture (fonts, images)
				struct vk_ui_image *uiimg = MTY_HashGetInt(cache, pcmd->texture);
				if (!uiimg)
					uiimg = ctx->clear_img;

				if (uiimg != prev_img) {
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->layout, 0, 1, &uiimg->dset, 0, NULL);
					prev_img = uiimg;
				}

				// Draw indexed
				vkCmdDrawIndexed(cmd, pcmd->elemCount, 1, pcmd->idxOffset + idxOffset,
					pcmd->vtxOffset + vtxOffset, 0);
			}
		}

		idxOffset += cmdList->idxLength;
		vtxOffset += cmdList->vtxLength;
	}

	// End render pass
	vkCmdEndRenderPass(cmd);

	return true;
}

void *mty_vk_ui_create_texture(struct gfx_ui *gfx_ui, struct gfx_device *device, const void *rgba,
	uint32_t width, uint32_t height)
{
	struct vk_ui *ctx = (struct vk_ui *) gfx_ui;

	struct vk_device_objects *dobjs = (struct vk_device_objects *) device;

	const VkPhysicalDeviceMemoryProperties *pdprops = dobjs->physicalDeviceMemoryProperties;
	VkDevice _device = dobjs->device;

	struct vk_ui_image *uiimg = MTY_Alloc(1, sizeof(struct vk_ui_image));
	bool r = vk_create_image(pdprops, _device, VK_FORMAT_R8G8B8A8_UNORM, width, width, height, 4, &uiimg->img);
	if (!r)
		goto except;

	r = vk_buffer_upload(_device, uiimg->img.buf.mem, rgba, width * height * 4);
	if (!r)
		goto except;

	VkDescriptorSetAllocateInfo dsai = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = ctx->dpool,
		.descriptorSetCount = 1,
		.pSetLayouts = &ctx->dlayout,
	};

	VkResult e = vkAllocateDescriptorSets(_device, &dsai, &uiimg->dset);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	VkWriteDescriptorSet dw = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = uiimg->dset,
		.dstBinding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.pImageInfo = &(VkDescriptorImageInfo) {
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.imageView = uiimg->img.view,
			.sampler = ctx->sampler,
		},
	};

	vkUpdateDescriptorSets(_device, 1, &dw, 0, NULL);

	uiimg->w = width;
	uiimg->h = height;

	except:

	if (!r) {
		vk_destroy_image(_device, &uiimg->img);

		MTY_Free(uiimg);
		uiimg = NULL;
	}

	return uiimg;
}

void mty_vk_ui_destroy_texture(struct gfx_ui *gfx_ui, void **texture, struct gfx_device *device)
{
	struct vk_ui *ctx = (struct vk_ui *) gfx_ui;
	if (!ctx)
		return;

	if (!texture || !*texture)
		return;

	struct vk_device_objects *dobjs = (struct vk_device_objects *) device;
	if (!dobjs)
		return;

	VkDevice _device = dobjs->device;
	if (!_device)
		return;

	struct vk_ui_image *uiimg = *texture;

	if (ctx->dpool && uiimg->dset)
		vkFreeDescriptorSets(_device, ctx->dpool, 1, &uiimg->dset);

	vk_destroy_image(_device, &uiimg->img);

	MTY_Free(uiimg);
	*texture = NULL;
}

void mty_vk_ui_destroy(struct gfx_ui **gfx_ui, struct gfx_device *device)
{
	if (!gfx_ui || !*gfx_ui)
		return;

	struct vk_ui *ctx = (struct vk_ui *) *gfx_ui;

	struct vk_device_objects *dobjs = (struct vk_device_objects *) device;

	if (dobjs) {
		VkDevice _device = dobjs->device;

		if (_device) {
			vk_ui_destroy_buffer(_device, &ctx->ib);
			vk_ui_destroy_buffer(_device, &ctx->vb);
		}

		mty_vk_ui_destroy_texture(*gfx_ui, (void **) &ctx->clear_img, device);

		if (ctx->pipeline)
			vkDestroyPipeline(_device, ctx->pipeline, NULL);

		if (ctx->layout)
			vkDestroyPipelineLayout(_device, ctx->layout, NULL);

		if (ctx->dlayout)
			vkDestroyDescriptorSetLayout(_device, ctx->dlayout, NULL);

		if (ctx->dpool)
			vkDestroyDescriptorPool(_device, ctx->dpool, NULL);

		if (ctx->sampler)
			vkDestroySampler(_device, ctx->sampler, NULL);

		if (ctx->rp_clear)
			vkDestroyRenderPass(_device, ctx->rp_clear, NULL);

		if (ctx->rp)
			vkDestroyRenderPass(_device, ctx->rp, NULL);

		if (ctx->frag)
			vkDestroyShaderModule(_device, ctx->frag, NULL);

		if (ctx->vert)
			vkDestroyShaderModule(_device, ctx->vert, NULL);
	}

	vkproc_global_destroy();

	MTY_Free(ctx);
	*gfx_ui = NULL;
}
