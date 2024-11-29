#pragma once
#include "../Rosy.h"
#include "rhi_shader.h"
#include "rhi_descriptor.h"
#include "rhi_types.h"

class rhi;

struct material_instance_result
{
	VkResult result;
	[[maybe_unused]] material_instance material;
};

struct gltf_metallic_roughness {
	shader_pipeline opaque_shaders;
	shader_pipeline transparent_shaders;

	VkDescriptorSetLayout material_layout;

	struct material_constants {
		glm::vec4 color_factors;
		glm::vec4 metal_rough_factors;
		glm::vec4 extra[14];
	};

	struct material_resources {
		allocated_image color_image;
		VkSampler color_sampler;
		allocated_image metal_rough_image;
		VkSampler metal_rough_sampler;
		VkBuffer data_buffer;
		uint32_t data_buffer_offset;
	};

	descriptor_writer writer;

	VkResult build_pipelines(VkDevice device, VkDescriptorSetLayout gpu_scene_descriptor_layout, allocated_image draw_image);
	void clear_resources(VkDevice device);

	material_instance_result write_material(VkDevice device, material_pass pass, const material_resources& resources, descriptor_allocator_growable& descriptor_allocator);
};
