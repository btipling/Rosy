#include "rhi_material.h"
#include "rhi_utils.h"
#include "rhi_helpers.h"

VkResult gltf_metallic_roughness::build_pipelines(const VkDevice device, const VkDescriptorSetLayout gpu_scene_descriptor_layout, allocated_image draw_image)
{
	std::vector<char> vert_shader_code;
	std::vector<char> frag_shader_code;
	try
	{
		vert_shader_code = read_file("out/mesh.vert.spv");
		frag_shader_code = read_file("out/mesh.frag.spv");
	}
	catch (const std::exception& e)
	{
		rosy_utils::debug_print_a("error reading shader files! %s", e.what());
		return VK_ERROR_UNKNOWN;
	}

	VkPushConstantRange matrix_range{};
	matrix_range.offset = 0;
	matrix_range.size = sizeof(gpu_draw_push_constants);
	matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	descriptor_layout_builder layout_builder;
	layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layout_builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	{
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return result;
		material_layout = set;
	}
	VkDescriptorSetLayout layouts[] = { gpu_scene_descriptor_layout, material_layout };

	shader_pipeline pipeline_builder;
	opaque_shaders.with_shaders(vert_shader_code, frag_shader_code);
	opaque_shaders.layouts = layouts;
	opaque_shaders.num_layouts = 2;
	opaque_shaders.shader_constants = &material_layout;
	opaque_shaders.shader_constants_size = sizeof(material_layout);
	transparent_shaders.with_shaders(vert_shader_code, frag_shader_code);
	transparent_shaders.blending = shader_blending::blending_additive;
	transparent_shaders.depth_enabled = false;
	transparent_shaders.layouts = layouts;
	transparent_shaders.num_layouts = 2;
	transparent_shaders.shader_constants = &matrix_range;
	transparent_shaders.shader_constants_size = sizeof(matrix_range);

	{
		if (const VkResult result = opaque_shaders.build(device); result != VK_SUCCESS) return result;
	}
	{
		if (const VkResult result = transparent_shaders.build(device); result != VK_SUCCESS) return result;
	}
	return VK_SUCCESS;
}

material_instance_result gltf_metallic_roughness::write_material(const VkDevice device, const material_pass pass, const material_resources& resources, descriptor_allocator_growable& descriptor_allocator)
{
	material_instance mat_data;
	mat_data.pass_type = pass;
	if (pass == material_pass::transparent) {
		mat_data.shaders = &transparent_shaders;
	}
	else {
		mat_data.shaders = &opaque_shaders;
	}
	{
		auto [result, set] = descriptor_allocator.allocate(device, material_layout);
		if (result != VK_SUCCESS) {
			material_instance_result rv;
			rv.result = result;
			return rv;
		}

		mat_data.material_set = set;
	}


	writer.clear();
	writer.write_buffer(0, resources.data_buffer, sizeof(material_constants), resources.data_buffer_offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, resources.color_image.image_view, resources.color_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(2, resources.metal_rough_image.image_view, resources.metal_rough_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.update_set(device, mat_data.material_set);
	{
		material_instance_result rv;
		rv.result = VK_SUCCESS;
		rv.material = mat_data;
		return rv;
	}
}

void gltf_metallic_roughness::deinit(const VkDevice device) const
{
	opaque_shaders.deinit(device);
	transparent_shaders.deinit(device);
	vkDestroyDescriptorSetLayout(device, material_layout, nullptr);

}
