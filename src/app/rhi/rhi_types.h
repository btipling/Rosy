#pragma once
#include <queue>
#include <fastgltf/types.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

class shader_pipeline;

namespace rh
{
	enum class result : std::uint8_t { ok, error };
	struct ctx;
}

struct gpu_scene_data
{
	glm::mat4 view = glm::mat4(1.f);
	glm::mat4 proj = glm::mat4(1.f);
	glm::mat4 view_projection = glm::mat4(1.f);
	glm::mat4 shadow_projection_near = glm::mat4(1.f);
	glm::mat4 shadow_projection_middle = glm::mat4(1.f);
	glm::mat4 shadow_projection_far = glm::mat4(1.f);
	glm::vec4 camera_position = glm::vec4(1.f);
	glm::vec4 ambient_color = glm::vec4(1.f);
	glm::vec4 sunlight_direction = glm::vec4(1.f);
	glm::vec4 sunlight_color = glm::vec4(1.f);
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

struct allocated_csm
{
	VkImage image;
	VkImageView image_view_near;
	VkImageView image_view_middle;
	VkImageView image_view_far;
	VmaAllocation allocation;
	VkExtent3D image_extent;
	VkFormat image_format;
};

struct allocated_ktx_image
{
	VkImage image;
	VkImageView image_view;
	VmaAllocation allocation;
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
	float texture_coordinates_x;
	glm::vec3 normal;
	float texture_coordinates_y;
	glm::vec4 color;
};

struct gpu_scene_buffers
{
	allocated_buffer scene_buffer;
	VkDeviceAddress scene_buffer_address;
	size_t buffer_size;
};

struct gpu_scene_buffers_result
{
	VkResult result;
	gpu_scene_buffers scene_buffers;
};

struct gpu_material_buffers
{
	allocated_buffer material_buffer;
	VkDeviceAddress material_buffer_address;
};

struct gpu_material_buffers_result
{
	VkResult result;
	gpu_material_buffers buffers;
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

struct gpu_render_buffers
{
	allocated_buffer render_buffer;
	VkDeviceAddress render_buffer_address;
	size_t buffer_size;
};

struct gpu_render_buffers_result
{
	VkResult result;
	gpu_render_buffers render_buffers;
};

struct gpu_draw_push_constants
{
	VkDeviceAddress scene_buffer{ 0 };
	VkDeviceAddress vertex_buffer{ 0 };
	VkDeviceAddress render_buffer{ 0 };
	VkDeviceAddress material_buffer{ 0 };
};

struct debug_draw_push_constants
{
	glm::mat4 world_matrix;
	glm::vec4 p1 = glm::vec4(0.f, 0.f, 0.f, 1.f);
	glm::vec4 p2 = glm::vec4(1.f, 0.f, 0.f, 1.f);
	glm::vec4 color;
};

struct geo_surface
{
	uint32_t start_index;
	uint32_t count;
	size_t material{ 0 };
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

struct render_data
{
	glm::mat4 transform;
	glm::mat3 normal_transform;
	glm::uvec4 material_data;
};

struct material_data
{
	glm::uint color_texture_index{0};
	glm::uint color_sampler_index{0};
};

struct render_object {
	uint32_t index_count;
	uint32_t first_index;
	VkBuffer index_buffer;

	size_t material_index;
	size_t mesh_index;
	bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress scene_buffer_address{ 0 };
	VkDeviceAddress vertex_buffer_address{ 0 };
	VkDeviceAddress render_buffer_address{ 0 };
	VkDeviceAddress material_buffer_address{ 0 };
};

enum class material_pass :uint8_t {
	main_color,
	transparent,
	other
};

struct material
{
	material_pass pass_type;
	size_t image_set_id{ 0 };
	size_t sampler_set_id{ 0 };
};

struct vulkan_ctx
{
	VkPhysicalDevice gpu;
	VkDevice device;
	VkQueue queue;
	VkCommandPool cmd_pool;
};

struct mesh_ctx
{
	const rh::ctx* ctx = nullptr;
	glm::mat4 world_transform = { 1.f };
	glm::mat4 view_proj = { 1.f };
	size_t scene_index{ 0 };
	bool wire_frame = false;
	bool depth_enabled = true;
	VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE;
	VkCommandBuffer shadow_pass_cmd;
	VkCommandBuffer render_cmd;
	VkExtent2D extent;
};

struct mesh_node
{
	std::vector<size_t>children;
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

class mesh_scene;

struct ktx_auto_texture
{
	ktxTexture* texture;
	ktxVulkanTexture vk_texture;
};


namespace ktx_sub_allocator
{
	void init_vma(const VmaAllocator& allocator);
	uint64_t alloc_mem_c_wrapper(VkMemoryAllocateInfo* alloc_info, VkMemoryRequirements* mem_req, uint64_t* num_pages);
	VkResult bind_buffer_memory_c_wrapper(VkBuffer buffer, uint64_t alloc_id);
	VkResult bind_image_memory_c_wrapper(VkImage image, uint64_t alloc_id);
	VkResult map_memory_c_wrapper(uint64_t alloc_id, uint64_t, VkDeviceSize* map_length, void** data_ptr);
	void unmap_memory_c_wrapper(uint64_t alloc_id, uint64_t);
	void free_mem_c_wrapper(uint64_t alloc_id);
}

class rhi;

class rhi_data
{
public:
	explicit rhi_data(rhi* renderer);
	rh::result load_gltf_meshes(const rh::ctx& ctx, std::filesystem::path file_path, mesh_scene& gltf_mesh_scene);
	[[nodiscard]] auto upload_mesh(std::span<uint32_t> indices, std::span<vertex> vertices) const->gpu_mesh_buffers_result;
	[[nodiscard]] auto upload_materials(std::span<material_data> materials) const->gpu_material_buffers_result;
	[[nodiscard]] auto create_render_data(size_t num_surfaces) const->gpu_render_buffers_result;
	[[nodiscard]] auto create_scene_data() const->gpu_scene_buffers_result;
	allocated_buffer_result create_buffer(const char* name, const size_t alloc_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage) const;


	void destroy_buffer(const allocated_buffer& buffer) const;

	[[nodiscard]] auto create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
		bool mip_mapped) const->allocated_image_result;
	std::expected<ktx_auto_texture, ktx_error_code_e> create_image(const void* data, const VkExtent3D size, const VkFormat format,
		const VkImageUsageFlags usage, const bool mip_mapped);
	std::expected<ktxVulkanTexture, ktx_error_code_e> create_image(ktxTexture* ktx_texture, const VkImageUsageFlags usage);
	std::expected<ktx_auto_texture, ktx_error_code_e> create_image(fastgltf::Asset& asset, const fastgltf::Image& image, VkFormat format);

	void destroy_image(const allocated_image& img) const;
	void destroy_image(ktx_auto_texture& img);

private:
	rhi* renderer_;
	ktxVulkanTexture_subAllocatorCallbacks sub_allocator_callbacks_{};
};