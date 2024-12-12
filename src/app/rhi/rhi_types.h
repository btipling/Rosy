#pragma once
#include <queue>
#include <fastgltf/types.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

class shader_pipeline;

struct gpu_scene_data
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 view_projection;
	glm::vec4 camera_position;
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
	glm::vec3 position;
	float texture_coordinates_s;
	glm::vec3 normal;
	float texture_coordinates_t;
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

struct glft_material;

struct geo_surface
{
	uint32_t start_index;
	uint32_t count;

	std::shared_ptr<glft_material> material = nullptr;
};

struct mesh_asset
{
	std::string name;

	std::vector<geo_surface> surfaces;
	gpu_mesh_buffers mesh_buffers;
};

struct bounds {
	glm::vec3 origin;
	float sphere_radius;
	glm::vec3 extents;
};

struct render_object {
	uint32_t index_count;
	uint32_t first_index;
	VkBuffer index_buffer;

	bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertex_buffer_address;
};

enum class material_pass :uint8_t {
	main_color,
	transparent,
	other
};

struct material_instance {
	shader_pipeline* shaders;
	VkDescriptorSet material_set;
	material_pass pass_type;
};

struct glft_material
{
	material_instance data;
};

struct vulkan_ctx
{
	VkPhysicalDevice gpu;
	VkDevice device;
	VkQueue queue;
	VkCommandPool cmd_pool;
};

struct mesh_node
{
	std::vector<size_t> children;
	glm::mat4 model_transform;
	glm::mat4 world_transform;

	std::optional<size_t> mesh_index = std::nullopt;

	void update(const glm::mat4& parent_transform, const std::vector<std::shared_ptr<mesh_node>>& meshes)
	{
		world_transform = parent_transform * model_transform;
		for (const auto i : children) {
			meshes[i]->update(world_transform, meshes);
		}
	}
};

struct texture_id {
	uint32_t index;
};

struct texture_cache
{
	std::vector<VkDescriptorImageInfo> cache;
	std::unordered_map<std::string, texture_id> name_map;
	texture_id add_texture(const VkImageView& image, VkSampler sampler);
};

struct mesh_scene
{
	size_t root_scene = 0;
	std::vector<std::vector<size_t>>scenes;
	std::vector<std::shared_ptr<mesh_node>> nodes;
	std::vector<std::shared_ptr<mesh_asset>> meshes;

	void add_node(fastgltf::Node& gltf_node);
	void add_scene(fastgltf::Scene& gltf_scene);
	[[nodiscard]] std::vector<render_object> draw_queue(const size_t scene_index, const glm::mat4& m = { 1.f }) const;
};

class rhi;

class rhi_data
{
public:
	explicit rhi_data(rhi* renderer);
	[[nodiscard]] std::optional<mesh_scene> load_gltf_meshes(std::filesystem::path file_path) const;
	[[nodiscard]] auto upload_mesh(std::span<uint32_t> indices, std::span<vertex> vertices) const -> gpu_mesh_buffers_result;
	allocated_buffer_result create_buffer(const char* name, const size_t alloc_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage) const;


	void destroy_buffer(const allocated_buffer& buffer) const;

	[[nodiscard]] auto create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
	                                bool mip_mapped) const -> allocated_image_result;
	std::expected<ktxVulkanTexture, ktx_error_code_e> create_image(const void* data, const VkExtent3D size, const VkFormat format,
		const VkImageUsageFlags usage, const bool mip_mapped) const;
	std::expected<ktxVulkanTexture, ktx_error_code_e> create_image(ktxTexture* ktx_texture, const VkImageUsageFlags usage) const;
	std::expected<ktxVulkanTexture, ktx_error_code_e> create_image(fastgltf::Asset& asset, fastgltf::Image& image) const;

	void destroy_image(const allocated_image& img) const;

private:
	rhi* renderer_;
	texture_cache texture_cache_{};
};