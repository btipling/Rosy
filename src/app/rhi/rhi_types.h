#pragma once

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

struct material_instance {
	shader_pipeline* shaders;
	VkDescriptorSet material_set;
	material_pass pass_type;
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
	//std::vector<std::unique_ptr<mesh_node>> children;
	glm::vec3 translation;
	glm::vec4 rotation;
	glm::vec3 scale;

	size_t mesh_index;
};

struct mesh_scene
{
	std::vector<std::unique_ptr<mesh_node>> nodes;
	std::vector<std::shared_ptr<mesh_asset>> meshes;
};

class rhi;

class rhi_data
{
public:
	explicit rhi_data(rhi* renderer);
	std::optional<mesh_scene> load_gltf_meshes(std::filesystem::path file_path) const;
	gpu_mesh_buffers_result upload_mesh(std::span<uint32_t> indices, std::span<vertex> vertices) const;
	allocated_buffer_result create_buffer(const char* name, const size_t alloc_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage) const;


	void destroy_buffer(const allocated_buffer& buffer) const;

	allocated_image_result create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
		bool mip_mapped) const;
	allocated_image_result create_image(const void* data, const VkExtent3D size, const VkFormat format,
		const VkImageUsageFlags usage, const bool mip_mapped) const;
	std::expected<ktxVulkanTexture, ktx_error_code_e> create_image(ktxTexture* ktx_texture, const VkImageUsageFlags usage) const;

	void destroy_image(const allocated_image& img) const;

private:
	rhi* renderer_;
};