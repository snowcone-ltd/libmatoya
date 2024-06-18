// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod.h"
GFX_PROTOTYPES(_vk_)

#include <string.h>

#include "gfx/viewport.h"
#include "gfx/fmt.h"
#include "vk-common.h"
#include "fmt-vk.h"

static
#include "shaders/fs.h"

static
#include "shaders/vs.h"

#define VK_NUM_STAGING 3
#define VK_NUM_DESC    (VK_NUM_STAGING + 1)

struct vk {
	MTY_ColorFormat format;

	VkShaderModule vert;
	VkShaderModule frag;
	VkSampler linear;
	VkSampler nearest;
	VkDescriptorSetLayout dlayout;
	VkDescriptorPool dpool;
	VkDescriptorSet dset;
	VkPipelineLayout layout;
	VkPipeline pipeline;
	VkRenderPass rp;

	struct gfx_uniforms cb;
	struct vk_buffer vb;
	struct vk_buffer ib;
	struct vk_buffer ub;

	struct vk_image staging[VK_NUM_STAGING];
};


// Main

static bool vk_one_shot_buffer(const VkPhysicalDeviceMemoryProperties *pdprops, VkDevice device,
	VkBufferUsageFlagBits usage, const void *data, size_t size, struct vk_buffer *buf)
{
	bool r = vk_allocate_buffer(pdprops, device, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, buf);
	if (!r)
		goto except;

	r = vk_buffer_upload(device, buf->mem, data, size);
	if (!r)
		goto except;

	except:

	if (!r)
		vk_destroy_buffer(device, buf);

	return r;
}

struct gfx *mty_vk_create(MTY_Device *device, uint8_t layer)
{
	struct vk *ctx = MTY_Alloc(1, sizeof(struct vk));

	MTY_VkDeviceObjects *dobjs = (MTY_VkDeviceObjects *) device;
	const VkPhysicalDeviceMemoryProperties *pdprops = dobjs->physicalDeviceMemoryProperties;
	VkDevice _device = dobjs->device;

	bool r = true;

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

	// Vertex buffer
	const float vertices[] = {
		-1.0f, -1.0f,	// Position 0 -- top left     (-1x, -1y)
		 0.0f,  0.0f,	// TexCoord 0 -- top left     ( 0x,  0y)

		 1.0f, -1.0f,	// Position 1 -- top right    ( 1x, -1y)
		 1.0f,  0.0f,	// TexCoord 1 -- top right    ( 1x,  0y)

		 1.0f,  1.0f,	// Position 2 -- bottom right ( 1x,  1y)
		 1.0f,  1.0f,	// TexCoord 2 -- bottom right ( 1x,  1y)

		-1.0f,  1.0f,	// Position 3 -- bottom left  (-1x,  1y)
		 0.0f,  1.0f	// TexCoord 3 -- bottom left  ( 0x,  1y)
	};

	r = vk_one_shot_buffer(pdprops, _device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices, sizeof(vertices), &ctx->vb);
	if (!r)
		goto except;

	// Index buffer
	const uint16_t indices[] = {
		0, 1, 2, 2, 3, 0
	};

	r = vk_one_shot_buffer(pdprops, _device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices, sizeof(indices), &ctx->ib);
	if (!r)
		goto except;

	// Uniform buffer
	r = vk_one_shot_buffer(pdprops, _device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &ctx->cb, sizeof(ctx->cb), &ctx->ub);
	if (!r)
		goto except;

	// Linear sampler
	VkSamplerCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	};

	e = vkCreateSampler(_device, &sci, NULL, &ctx->linear);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Nearest sampler
	sci.magFilter = sci.minFilter = VK_FILTER_NEAREST;

	e = vkCreateSampler(_device, &sci, NULL, &ctx->nearest);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Render pass
	VkAttachmentDescription ad = {
		.format = VKPROC_FORMAT,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = layer == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR :
			VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = layer == 0 ? VK_IMAGE_LAYOUT_UNDEFINED :
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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

	// Descriptor set layout
	VkDescriptorSetLayoutCreateInfo lci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = VK_NUM_DESC,
		.pBindings = (VkDescriptorSetLayoutBinding []) {
			{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
			{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
			{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
			{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1, VK_SHADER_STAGE_FRAGMENT_BIT},
		},
	};

	e = vkCreateDescriptorSetLayout(_device, &lci, NULL, &ctx->dlayout);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Descriptor pool
	VkDescriptorPoolCreateInfo dpi = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.poolSizeCount = 2,
		.pPoolSizes = (VkDescriptorPoolSize []) {{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = VK_NUM_STAGING,
		}, {
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
		}},
		.maxSets = 1,
	};

	e = vkCreateDescriptorPool(_device, &dpi, NULL, &ctx->dpool);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Descriptor set
	VkDescriptorSetAllocateInfo dsai = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = ctx->dpool,
		.descriptorSetCount = 1,
		.pSetLayouts = &ctx->dlayout,
	};

	e = vkAllocateDescriptorSets(_device, &dsai, &ctx->dset);
	if (e != VK_SUCCESS) {
		r = false;
		goto except;
	}

	// Pipeline layout
	VkPipelineLayoutCreateInfo pci = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &ctx->dlayout,
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
				.stride = 4 * sizeof(float),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
			},
			.vertexAttributeDescriptionCount = 2,
			.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription []) {{
				.location = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = 0,
			}, {
				.location = 1,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = 2 * sizeof(float),
			}},
		},
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		},
		.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.lineWidth = 1.0f,
			.cullMode = VK_CULL_MODE_BACK_BIT,
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
				.blendEnable = layer == 0 ? VK_FALSE : VK_TRUE,
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

	except:

	if (!r)
		mty_vk_destroy((struct gfx **) &ctx, device);

	return (struct gfx *) ctx;
}

static bool vk_refresh_image(struct gfx *gfx, MTY_Device *device, MTY_Context *context, MTY_ColorFormat fmt,
	uint8_t plane, const uint8_t *image, uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp)
{
	struct vk *ctx = (struct vk *) gfx;

	bool r = true;

	struct vk_image *img = &ctx->staging[plane];
	VkFormat format = FMT_PLANES[fmt][plane];

	MTY_VkDeviceObjects *dobjs = (MTY_VkDeviceObjects *) device;
	const VkPhysicalDeviceMemoryProperties *pdprops = dobjs->physicalDeviceMemoryProperties;
	VkDevice _device = dobjs->device;

	VkCommandBuffer cmd = (VkCommandBuffer) context;

	// Resize
	if (!img->image || w != img->w || h != img->h || format != img->format) {
		vk_destroy_image(_device, img);

		r = vk_create_image(pdprops, _device, format, full_w, w, h, bpp, img);
		if (!r)
			goto except;
	}

	except:

	if (r) {
		// Upload to staging buffer
		r = vk_buffer_upload(_device, img->buf.mem, image, full_w * h * bpp);

		// GPU copy from staging
		if (r)
			vk_image_gpu_copy(img, cmd, full_w, w, h);

	} else {
		vk_destroy_image(_device, img);
	}

	return r;
}

bool mty_vk_valid_hardware_frame(MTY_Device *device, MTY_Context *context, const void *shared_resource)
{
	return false;
}

bool mty_vk_render(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest)
{
	struct vk *ctx = (struct vk *) gfx;

	MTY_VkDeviceObjects *dobjs = (MTY_VkDeviceObjects *) device;
	VkDevice _device = dobjs->device;

	VkCommandBuffer cmd = (VkCommandBuffer) context;
	VkFramebuffer fb = (VkFramebuffer) dest;

	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	// If format == MTY_COLOR_FORMAT_UNKNOWN, texture refreshing/loading is skipped and the previous frame is rendered
	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	// Refresh textures and load texture data
	if (!fmt_reload_textures(gfx, device, context, image, desc, vk_refresh_image))
		return false;

	// Begin render pass
	VkRenderPassBeginInfo rpi = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = ctx->rp,
		.framebuffer = fb,
		.renderArea.extent = {
			.width = desc->viewWidth,
			.height = desc->viewHeight,
		},
		.clearValueCount = 1,
		.pClearValues = &(VkClearValue) {
			.color.float32 = {0, 0, 0, 1}
		},
	};

	vkCmdBeginRenderPass(cmd, &rpi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline);

	// Viewport
	VkViewport vp = {.maxDepth = 1.0f};
	mty_viewport(desc, &vp.x, &vp.y, &vp.width, &vp.height);

	vkCmdSetViewport(cmd, 0, 1, &vp);

	// Scissor
	VkRect2D scissor = {
		.extent.width = desc->viewWidth,
		.extent.height = desc->viewHeight,
	};

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// Update uniform buffer and descriptor
	struct gfx_uniforms cb =  {
		.width = (float) desc->cropWidth,
		.height = (float) desc->cropHeight,
		.vp_height = vp.height,
		.effects[0] = desc->effects[0],
		.effects[1] = desc->effects[1],
		.levels[0] = desc->levels[0],
		.levels[1] = desc->levels[1],
		.planes = FMT_INFO[ctx->format].planes,
		.rotation = desc->rotation,
		.conversion = FMT_CONVERSION(ctx->format, desc->fullRangeYUV, desc->multiplyYUV),
	};

	if (memcmp(&ctx->cb, &cb, sizeof(struct gfx_uniforms))) {
		if (!vk_buffer_upload(_device, ctx->ub.mem, &cb, sizeof(struct gfx_uniforms)))
			return false;

		VkWriteDescriptorSet dw = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = ctx->dset,
			.dstBinding = VK_NUM_STAGING,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.pBufferInfo = &(VkDescriptorBufferInfo) {
				.buffer = ctx->ub.buf,
				.range = VK_WHOLE_SIZE,
			},
		};

		vkUpdateDescriptorSets(_device, 1, &dw, 0, NULL);

		ctx->cb = cb;
	}

	// Update image descriptors
	VkDescriptorImageInfo ii[VK_NUM_STAGING] = {0};
	VkWriteDescriptorSet dw[VK_NUM_DESC] = {0};

	for (uint8_t x = 0; x < VK_NUM_STAGING; x++) {
		// Vulkan must have a texture bound to each slot -- there should always be at least one
		ii[x].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ii[x].imageView = ctx->staging[x].view ? ctx->staging[x].view : ctx->staging[0].view;
		ii[x].sampler = desc->filter == MTY_FILTER_NEAREST ? ctx->nearest : ctx->linear;

		dw[x].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		dw[x].dstSet = ctx->dset;
		dw[x].dstBinding = x;
		dw[x].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		dw[x].descriptorCount = 1;
		dw[x].pImageInfo = &ii[x];
	}

	vkUpdateDescriptorSets(_device, VK_NUM_STAGING, dw, 0, NULL);

	// Bind vertex/index bufers, uniforms and draw
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &ctx->vb.buf, &offset);
	vkCmdBindIndexBuffer(cmd, ctx->ib.buf, 0, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		ctx->layout, 0, 1, &ctx->dset, 0, NULL);

	vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

	// End render pass
	vkCmdEndRenderPass(cmd);

	return true;
}

void mty_vk_clear(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	uint32_t width, uint32_t height, float r, float g, float b, float a, MTY_Surface *dest)
{
	struct vk *ctx = (struct vk *) gfx;

	VkCommandBuffer cmd = (VkCommandBuffer) context;
	VkFramebuffer fb = (VkFramebuffer) dest;

	VkRenderPassBeginInfo rpi = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = ctx->rp,
		.framebuffer = fb,
		.renderArea.extent = {
			.width = width,
			.height = height,
		},
		.clearValueCount = 1,
		.pClearValues = &(VkClearValue) {
			.color.float32 = {r, g, b, a}
		},
	};

	vkCmdBeginRenderPass(cmd, &rpi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(cmd);
}

void mty_vk_destroy(struct gfx **gfx, MTY_Device *device)
{
	if (!gfx || !*gfx)
		return;

	struct vk *ctx = (struct vk *) *gfx;

	MTY_VkDeviceObjects *dobjs = (MTY_VkDeviceObjects *) device;

	if (dobjs) {
		VkDevice _device = dobjs->device;

		if (_device) {
			for (uint8_t x = 0; x < VK_NUM_STAGING; x++)
				vk_destroy_image(_device, &ctx->staging[x]);

			if (ctx->pipeline)
				vkDestroyPipeline(_device, ctx->pipeline, NULL);

			if (ctx->layout)
				vkDestroyPipelineLayout(_device, ctx->layout, NULL);

			if (ctx->dpool) {
				if (ctx->dset)
					vkFreeDescriptorSets(_device, ctx->dpool, 1, &ctx->dset);

				vkDestroyDescriptorPool(_device, ctx->dpool, NULL);
			}

			if (ctx->dlayout)
				vkDestroyDescriptorSetLayout(_device, ctx->dlayout, NULL);

			if (ctx->rp)
				vkDestroyRenderPass(_device, ctx->rp, NULL);

			if (ctx->nearest)
				vkDestroySampler(_device, ctx->nearest, NULL);

			if (ctx->linear)
				vkDestroySampler(_device, ctx->linear, NULL);

			vk_destroy_buffer(_device, &ctx->ub);
			vk_destroy_buffer(_device, &ctx->ib);
			vk_destroy_buffer(_device, &ctx->vb);

			if (ctx->frag)
				vkDestroyShaderModule(_device, ctx->frag, NULL);

			if (ctx->vert)
				vkDestroyShaderModule(_device, ctx->vert, NULL);
		}
	}

	vkproc_global_destroy();

	MTY_Free(ctx);
	*gfx = NULL;
}
