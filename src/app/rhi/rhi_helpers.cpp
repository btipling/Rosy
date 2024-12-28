#include "rhi_helpers.h"

namespace rhi_helpers {

	VkImageCreateInfo img_create_info(const VkFormat format, const VkImageUsageFlags usage_flags,
		const VkExtent3D extent)
	{
		VkImageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = format;
		info.extent = extent;
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = usage_flags;
		return info;
	}

	VkImageViewCreateInfo img_view_create_info(const VkFormat format, const VkImage image, const VkImageAspectFlags aspect_flags)
	{
		VkImageViewCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.pNext = nullptr;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.image = image;
		info.format = format;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;
		info.subresourceRange.aspectMask = aspect_flags;
		return info;
	}

	VkRenderingAttachmentInfo attachment_info(const VkImageView view, const VkImageLayout layout)
	{
		VkRenderingAttachmentInfo color_attachment{};
		color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		color_attachment.pNext = nullptr;
		color_attachment.imageView = view;
		color_attachment.imageLayout = layout;
		color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		return color_attachment;
	}

	VkRenderingAttachmentInfo shadow_attachment_info(const VkImageView view, const VkImageLayout layout)
	{
		VkRenderingAttachmentInfo depth_attachment{};
		depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_attachment.pNext = nullptr;
		depth_attachment.imageView = view;
		depth_attachment.imageLayout = layout;
		depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment.clearValue.depthStencil.depth = 0.0f;
		return depth_attachment;
	}

	VkRenderingAttachmentInfo depth_attachment_info(const VkImageView view, const VkImageLayout layout)
	{
		VkRenderingAttachmentInfo depth_attachment{};
		depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_attachment.pNext = nullptr;
		depth_attachment.imageView = view;
		depth_attachment.imageLayout = layout;
		depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment.clearValue.depthStencil.depth = 0.0f;
		return depth_attachment;
	}

	VkImageCreateInfo shadow_img_create_info(const VkFormat format, const VkImageUsageFlags usage_flags,
		const VkExtent3D extent)
	{
		VkImageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = format;
		info.extent = extent;
		info.mipLevels = 1;
		info.arrayLayers = 3;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = usage_flags;
		return info;
	}

	VkImageViewCreateInfo shadow_img_view_create_info(const VkFormat format, const VkImage image, const VkImageAspectFlags aspect_flags)
	{
		VkImageViewCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.pNext = nullptr;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		info.image = image;
		info.format = format;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		info.subresourceRange.aspectMask = aspect_flags;
		return info;
	}

	VkRenderingInfo shadow_map_rendering_info(const VkExtent2D render_extent, const VkRenderingAttachmentInfo& depth_attachment)
	{
		const auto render_area = VkRect2D{ VkOffset2D{0, 0}, render_extent };
		VkRenderingInfo render_info{};
		render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		render_info.pNext = nullptr;
		render_info.renderArea = render_area;
		render_info.layerCount = 1;
		render_info.viewMask = 0b00000111;
		render_info.colorAttachmentCount = 0;
		render_info.pColorAttachments = nullptr;
		render_info.pDepthAttachment = &depth_attachment;
		render_info.pStencilAttachment = nullptr;
		return render_info;
	}

	VkImageSubresourceRange create_shadow_img_subresource_range(const VkImageAspectFlags aspect_mask)
	{
		VkImageSubresourceRange subresource_range{};
		subresource_range.aspectMask = aspect_mask;
		subresource_range.baseMipLevel = 0;
		subresource_range.levelCount = 1;
		subresource_range.baseArrayLayer = 0;
		subresource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
		return subresource_range;
	}

	VkRenderingInfo rendering_info(const VkExtent2D render_extent, const VkRenderingAttachmentInfo& color_attachment,
		const std::optional<VkRenderingAttachmentInfo>& depth_attachment)
	{
		const auto render_area = VkRect2D{ VkOffset2D{0, 0}, render_extent };
		VkRenderingInfo render_info{};
		render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		render_info.pNext = nullptr;
		render_info.renderArea = render_area;
		render_info.layerCount = 1;
		render_info.colorAttachmentCount = 1;
		render_info.pColorAttachments = &color_attachment;
		if (depth_attachment.has_value())
		{
			render_info.pDepthAttachment = &depth_attachment.value();
		}
		else
		{
			render_info.pDepthAttachment = nullptr;
		}
		render_info.pStencilAttachment = nullptr;
		return render_info;
	}


	void blit_images(const VkCommandBuffer cmd, const VkImage source, const VkImage destination,
		const VkExtent2D src_size, const VkExtent2D dst_size)
	{
		VkImageBlit2 blit_region{};
		blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
		blit_region.pNext = nullptr;
		blit_region.srcOffsets[1].x = static_cast<int32_t>(src_size.width);
		blit_region.srcOffsets[1].y = static_cast<int32_t>(src_size.height);
		blit_region.srcOffsets[1].z = 1;

		blit_region.dstOffsets[1].x = static_cast<int32_t>(dst_size.width);
		blit_region.dstOffsets[1].y = static_cast<int32_t>(dst_size.height);
		blit_region.dstOffsets[1].z = 1;

		blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit_region.srcSubresource.baseArrayLayer = 0;
		blit_region.srcSubresource.layerCount = 1;
		blit_region.srcSubresource.mipLevel = 0;

		blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit_region.dstSubresource.baseArrayLayer = 0;
		blit_region.dstSubresource.layerCount = 1;
		blit_region.dstSubresource.mipLevel = 0;

		VkBlitImageInfo2 blit_info{};
		blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
		blit_info.pNext = nullptr;
		blit_info.dstImage = destination;
		blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		blit_info.srcImage = source;
		blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		blit_info.filter = VK_FILTER_LINEAR;
		blit_info.regionCount = 1;
		blit_info.pRegions = &blit_region;

		vkCmdBlitImage2(cmd, &blit_info);
	}

	VkDebugUtilsObjectNameInfoEXT add_name(const VkObjectType object_type, const uint64_t object_handle,
		const char* p_object_name)
	{
		VkDebugUtilsObjectNameInfoEXT debug_name{};
		debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		debug_name.pNext = nullptr;
		debug_name.objectType = object_type;
		debug_name.objectHandle = object_handle;
		debug_name.pObjectName = p_object_name;
		return debug_name;
	}


	VkShaderCreateInfoEXT create_shader_info(const std::vector<char>& shader_src, const VkShaderStageFlagBits stage,
		const VkShaderStageFlags next_stage, const VkShaderCreateFlagsEXT shader_flags)
	{
		VkShaderCreateInfoEXT info{};
		info.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
		info.pNext = nullptr;
		info.flags = shader_flags;
		info.stage = stage;
		info.nextStage = next_stage;
		info.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
		info.codeSize = shader_src.size();
		info.pCode = shader_src.data();
		info.pName = "main";
		info.setLayoutCount = 0;
		info.pSetLayouts = nullptr;
		info.pushConstantRangeCount = 0;
		info.pPushConstantRanges = nullptr;
		info.pSpecializationInfo = nullptr;
		return info;
	}

	VkPushConstantRange create_push_constant(const VkShaderStageFlags stage, const uint32_t size)
	{
		VkPushConstantRange push_constant_range{};
		push_constant_range.stageFlags = stage;
		push_constant_range.offset = 0;
		push_constant_range.size = size;
		return push_constant_range;
	}

	VkWriteDescriptorSet create_img_write_descriptor_set(const VkDescriptorSet des_set, uint32_t des_binding, const VkDescriptorImageInfo& img_info)
	{
		VkWriteDescriptorSet desc_set{};
		desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc_set.pNext = nullptr;
		desc_set.dstBinding = 0;
		desc_set.dstSet = des_set;
		desc_set.descriptorCount = 1;
		desc_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		desc_set.pImageInfo = &img_info;
		return desc_set;
	}

	VkWriteDescriptorSet create_buffer_write_descriptor_set(const VkDescriptorSet des_set, uint32_t des_binding, const VkDescriptorBufferInfo& buf_info)
	{
		VkWriteDescriptorSet desc_set{};
		desc_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc_set.pNext = nullptr;
		desc_set.dstBinding = 0;
		desc_set.dstSet = des_set;
		desc_set.descriptorCount = 1;
		desc_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		desc_set.pBufferInfo = &buf_info;
		return desc_set;
	}

	VkDescriptorImageInfo create_img_descriptor_info(const VkImageView& image_view)
	{
		VkDescriptorImageInfo img_info{};
		img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		img_info.imageView = image_view;
		return img_info;
	}

	VkDebugUtilsLabelEXT create_debug_label(const char* label_name, float color[4])
	{
		VkDebugUtilsLabelEXT debug_label{};
		debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		debug_label.pNext = nullptr;
		debug_label.pLabelName = label_name;
		std::copy_n(color, 4, debug_label.color);
		return debug_label;
	}

	VkImageSubresourceRange create_img_subresource_range(const VkImageAspectFlags aspect_mask)
	{
		VkImageSubresourceRange subresource_range{};
		subresource_range.aspectMask = aspect_mask;
		subresource_range.baseMipLevel = 0;
		subresource_range.levelCount = VK_REMAINING_MIP_LEVELS;
		subresource_range.baseArrayLayer = 0;
		subresource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
		return subresource_range;
	}

	VkPipelineLayoutCreateInfo create_pipeline_layout_create_info(const VkPushConstantRange& pc_range, const uint32_t pc_count, const VkDescriptorSetLayout* set_layouts, const uint32_t sl_count)
	{
		VkPipelineLayoutCreateInfo pl_info{};
		pl_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pl_info.pNext = nullptr;
		pl_info.pushConstantRangeCount = pc_count;
		pl_info.pPushConstantRanges = &pc_range;
		pl_info.setLayoutCount = sl_count;
		pl_info.pSetLayouts = set_layouts;
		return pl_info;
	}

	VkClearAttachment create_clear_attachment()
	{
		VkClearColorValue clear_color_value{};
		clear_color_value.float32[0] = 1.f;
		clear_color_value.float32[1] = 0.f;
		clear_color_value.float32[2] = 0.f;
		clear_color_value.float32[3] = 0.f;
		clear_color_value.int32[0] = 1;
		clear_color_value.int32[1] = 0;
		clear_color_value.int32[2] = 0;
		clear_color_value.int32[3] = 0;
		clear_color_value.uint32[0] = 1;
		clear_color_value.uint32[1] = 0;
		clear_color_value.uint32[2] = 0;
		clear_color_value.uint32[3] = 0;

		VkClearDepthStencilValue clear_depth_stencil_value{};
		clear_depth_stencil_value.depth = 0.f;
		clear_depth_stencil_value.stencil = 0;

		VkClearValue clear_value{};
		clear_value.color = clear_color_value;
		clear_value.depthStencil = clear_depth_stencil_value;

		VkClearAttachment attachment{};
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		attachment.colorAttachment = 0;
		attachment.clearValue = clear_value;
		return attachment;
	}

	VkClearRect create_clear_rectangle(const VkExtent3D img_extent)
	{
		VkOffset2D offset{};
		offset.x = 0;
		offset.y = 0;

		VkExtent2D extent{};
		extent.width = img_extent.width;
		extent.height = img_extent.height;

		VkRect2D rect{};
		rect.offset = offset;
		rect.extent = extent;

		VkClearRect clear_rect{};
		clear_rect.rect = rect;
		clear_rect.baseArrayLayer = 0;
		clear_rect.layerCount = 1;
		return clear_rect;
	}
}