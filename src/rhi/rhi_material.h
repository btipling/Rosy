#pragma once
#include "../Rosy.h"

class rhi;

struct gltf_metallic_roughness {
	material_pipeline opaque_pipeline;
	material_pipeline transparent_pipeline;

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

	void build_pipelines(rhi* engine);
	void clear_resources(VkDevice device);

	material_instance write_material(VkDevice device, material_pass pass, const material_resources& resources, descriptor_allocator_growable& descriptor_allocator);
};
