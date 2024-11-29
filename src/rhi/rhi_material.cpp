#include "rhi.h"
//#include "rhi_material.h"
//#include "rhi_utils.h"
//#include "rhi_helpers.h"
//
//VkResult gltf_metallic_roughness::build_pipelines(VkDevice device, VkDescriptorSetLayout gpu_scene_descriptor_layout, allocated_image draw_image)
//{
//	std::vector<char> vert_shader_code;
//	std::vector<char> frag_shader_code;
//	try
//	{
//		vert_shader_code = read_file("out/mesh.vert.spv");
//		frag_shader_code = read_file("out/mesh.frag.spv");
//	}
//	catch (const std::exception& e)
//	{
//		rosy_utils::debug_print_a("error reading shader files! %s", e.what());
//		return VK_ERROR_UNKNOWN;
//	}
//
//	VkPushConstantRange matrix_range{};
//	matrix_range.offset = 0;
//	matrix_range.size = sizeof(gpu_draw_push_constants);
//	matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
//
//	descriptor_layout_builder layout_builder;
//	layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
//	layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//	layout_builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
//
//	{
//		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
//		if (result != VK_SUCCESS) return result;
//		material_layout = set;
//	}
//	const VkDescriptorSetLayout layouts[] = { gpu_scene_descriptor_layout, material_layout };
//
//	const VkPipelineLayoutCreateInfo mesh_layout_info = rhi_helpers::create_pipeline_layout_create_info(matrix_range, 1, layouts, 2);
//
//	VkPipelineLayout new_layout;
//	{
//		if (const VkResult result = vkCreatePipelineLayout(device, &mesh_layout_info, nullptr, &new_layout); result != VK_SUCCESS) return result;
//	}
//
//	opaque_shaders.pipeline_layout = new_layout;
//	transparent_shaders.pipeline_layout = new_layout;
//
//	shader_pipeline pipeline_builder;
//	opaque_shaders.with_shaders(vert_shader_code, frag_shader_code);
//	transparent_shaders.with_shaders(vert_shader_code, frag_shader_code);
//	transparent_shaders.blending = shader_blending::blending_additive;
//	transparent_shaders.depth_enabled = false;
//
//	{
//		if (const VkResult result = opaque_shaders.build(device); result != VK_SUCCESS) return result;
//	}
//	{
//		if (const VkResult result = transparent_shaders.build(device); result != VK_SUCCESS) return result;
//	}
//}