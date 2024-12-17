#include "rhi_shader.h"

void shader_pipeline::with_shaders(const std::vector<char>& vert, const std::vector<char>& frag)
{
	const VkShaderCreateInfoEXT vert_object = rhi_helpers::create_shader_info(vert, VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT);
	shaders_create_info_.push_back(vert_object);
	const VkShaderCreateInfoEXT frag_object = rhi_helpers::create_shader_info(frag, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	shaders_create_info_.push_back(frag_object);
}

VkResult shader_pipeline::build(const VkDevice device)
{

	VkPushConstantRange push_constant_range = rhi_helpers::create_push_constant(VK_SHADER_STAGE_VERTEX_BIT, shader_constants_size);
	push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	for (VkShaderCreateInfoEXT& create_info : shaders_create_info_)
	{
		create_info.setLayoutCount = layouts.size();
		create_info.pSetLayouts = layouts.data();
		create_info.pushConstantRangeCount = 1;
		create_info.pPushConstantRanges = &push_constant_range;
	}
	shaders.resize(2);
	if (const VkResult result = vkCreateShadersEXT(device, shaders_create_info_.size(), shaders_create_info_.data(), nullptr, shaders.data()); result != VK_SUCCESS) return result;
	{
		const auto obj_name = std::format("{}_vert", name);
		const VkDebugUtilsObjectNameInfoEXT vert_name = rhi_helpers::add_name(VK_OBJECT_TYPE_SHADER_EXT, reinterpret_cast<uint64_t>(shaders.data()[0]), obj_name.c_str());
		if (const VkResult result = vkSetDebugUtilsObjectNameEXT(device, &vert_name); result != VK_SUCCESS) return result;
	}
	{
		const auto obj_name = std::format("{}_frag", name);
		const VkDebugUtilsObjectNameInfoEXT frag_name = rhi_helpers::add_name(VK_OBJECT_TYPE_SHADER_EXT, reinterpret_cast<uint64_t>(shaders.data()[1]), obj_name.c_str());
		if (const VkResult result = vkSetDebugUtilsObjectNameEXT(device, &frag_name); result != VK_SUCCESS) return result;
	}
	{
		const VkPipelineLayoutCreateInfo pl_info = rhi_helpers::create_pipeline_layout_create_info(push_constant_range, 1, layouts.data(), layouts.size());
		VkPipelineLayout layout{};
		if (const VkResult result = vkCreatePipelineLayout(device, &pl_info, nullptr, &layout); result != VK_SUCCESS) return result;
		pipeline_layout = layout;
	}
	return VK_SUCCESS;
}

VkResult shader_pipeline::shade(const VkCommandBuffer cmd) const
{
	rhi_cmd::set_rendering_defaults(cmd, primitive);
	rhi_cmd::toggle_culling(cmd, culling_enabled, front_face);
	rhi_cmd::toggle_wire_frame(cmd, wire_frames_enabled);
	rhi_cmd::set_view_port(cmd, viewport_extent);
	rhi_cmd::toggle_depth(cmd, depth_enabled);
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (blending)
	{
	case shader_blending::blending_disabled:
		rhi_cmd::disable_blending(cmd);
		break;
	case shader_blending::blending_additive:
		rhi_cmd::enable_blending_additive(cmd);
		break;
	case shader_blending::blending_alpha_blend:
		rhi_cmd::enable_blending_alpha_blend(cmd);
		break;
	}
	constexpr VkShaderStageFlagBits stages[2] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT
	};
	vkCmdBindShadersEXT(cmd, 2, stages, shaders.data());

	constexpr VkShaderStageFlagBits unused_stages[3] =
	{
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT
	};
	vkCmdBindShadersEXT(cmd, 3, unused_stages, nullptr);
	return VK_SUCCESS;
}

VkResult shader_pipeline::push(const VkCommandBuffer cmd) const
{
	vkCmdPushConstants(cmd, pipeline_layout.value(), VK_SHADER_STAGE_VERTEX_BIT, 0, shader_constants_size, shader_constants);
	return VK_SUCCESS;
}

void shader_pipeline::deinit(const VkDevice device) const
{
	if (pipeline_layout.has_value())
	{
		vkDestroyPipelineLayout(device, pipeline_layout.value(), nullptr);
	}

	for (const VkShaderEXT shader : shaders)
	{
		vkDestroyShaderEXT(device, shader, nullptr);
	}
}
