#pragma once

#include "rhi_descriptor.h"

struct gpu_scene_data
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 view_projection;
	glm::vec4 ambient_color;
	glm::vec4 sunlight_direction; // w for light intensity
	glm::vec4 sunlight_color;
};

struct swap_chain_support_details
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};

struct allocated_image
{
	VkImage image;
	VkImageView image_view;
	VmaAllocation allocation;
	VkExtent3D image_extent;
	VkFormat image_format;
};

struct allocated_image_result
{
	VkResult result;
	allocated_image image;
};

struct allocated_buffer
{
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct allocated_buffer_result
{
	VkResult result;
	allocated_buffer buffer;
};

struct vertex
{
	glm::vec4 position;
	glm::vec4 texture_coordinates;
	glm::vec4 normal;
	glm::vec4 color;
};

struct gpu_mesh_buffers
{
	allocated_buffer index_buffer;
	allocated_buffer vertex_buffer;
	VkDeviceAddress vertex_buffer_address;
};

struct gpu_mesh_buffers_result
{
	VkResult result;
	gpu_mesh_buffers buffers;
};

struct gpu_draw_push_constants
{
	glm::mat4 world_matrix;
	VkDeviceAddress vertex_buffer;
};

struct geo_surface
{
	uint32_t start_index;
	uint32_t count;
};

struct mesh_asset
{
	std::string name;

	std::vector<geo_surface> surfaces;
	gpu_mesh_buffers mesh_buffers;
};

enum class material_pass :uint8_t {
	main_color,
	transparent,
	other
};

struct material_pipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct material_instance {
	material_pipeline* pipeline;
	VkDescriptorSet material_set;
	material_pass pass_type;
};