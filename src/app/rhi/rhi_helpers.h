#pragma once
#include "../../Rosy.h"

namespace rhi_helpers {
	VkImageCreateInfo img_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent);
	VkImageViewCreateInfo img_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);
	VkRenderingAttachmentInfo attachment_info(VkImageView view, VkImageLayout layout);
	VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, VkImageLayout layout);
	VkRenderingInfo rendering_info(VkExtent2D render_extent, const VkRenderingAttachmentInfo& color_attachment,
		const std::optional<VkRenderingAttachmentInfo>& depth_attachment);
	void blit_images(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size,
		VkExtent2D dst_size);
	VkDebugUtilsObjectNameInfoEXT add_name(VkObjectType object_type, uint64_t object_handle,
		const char* p_object_name);
	VkDebugUtilsLabelEXT create_debug_label(const char* label_name, float color[4]);

	VkShaderCreateInfoEXT create_shader_info(const std::vector<char>& shader_src, const VkShaderStageFlagBits stage, const VkShaderStageFlags next_stage);
	VkPushConstantRange create_push_constant(VkShaderStageFlags stage, uint32_t size);
	VkWriteDescriptorSet create_img_write_descriptor_set(VkDescriptorSet des_set, uint32_t des_binding, const VkDescriptorImageInfo& img_info);
	VkWriteDescriptorSet create_buffer_write_descriptor_set(const VkDescriptorSet des_set, uint32_t des_binding, const VkDescriptorBufferInfo& buf_info);
	VkDescriptorImageInfo create_img_descriptor_info(const VkImageView& image_view);
	VkImageSubresourceRange create_img_subresource_range(VkImageAspectFlags aspect_mask);
	VkPipelineLayoutCreateInfo create_pipeline_layout_create_info(const VkPushConstantRange& pc_range, uint32_t pc_count, const VkDescriptorSetLayout* set_layouts, uint32_t sl_count);
}