#pragma once
#include "../Rosy.h"
#include "../loader/loader.h"

#define MAX_FRAMES_IN_FLIGHT 2

class rhi
{
public:
	explicit rhi(rosy_config::Config cfg);
	VkResult init(SDL_Window* window);
	VkResult resize_swapchain(SDL_Window* window);
	void deinit();
	VkResult draw_ui();
	VkResult draw_frame();
	// Buffer read write
	gpu_mesh_buffers_result upload_mesh(std::span<uint32_t> indices, std::span<vertex> vertices);
	gpu_scene_data scene_data;
	void debug() const;
	~rhi();

private:
	bool deinited_ = false;
	rosy_config::Config cfg_;
	std::vector<const char*> instance_layer_properties_;
	std::vector<const char*> device_layer_properties_;
	std::vector<const char*> instance_extensions_;
	std::vector<const char*> device_device_extensions_;
	std::optional<VkInstance> instance_ = std::nullopt;
	std::optional<VkPhysicalDevice> physical_device_ = std::nullopt;
	std::optional<VkPhysicalDeviceProperties> physical_device_properties_ = std::nullopt;
	std::optional<VkPhysicalDeviceFeatures> supported_features_ = std::nullopt;
	std::optional<VkPhysicalDeviceMemoryProperties> physical_device_memory_properties_ = std::nullopt;
	std::optional<std::vector<VkQueueFamilyProperties>> queue_family_properties_ = std::nullopt;
	std::uint32_t queue_index_ = 0;
	std::uint32_t queue_count_ = 0;
	std::vector<float> queue_priorities_;
	VkPhysicalDeviceFeatures required_features_;
	std::optional<VkDevice> device_ = std::nullopt;
	std::optional<VmaAllocator> allocator_ = std::nullopt;
	std::optional<VkSurfaceKHR> surface_ = std::nullopt;
	std::optional<VkQueue> present_queue_ = std::nullopt;
	std::optional<VkSwapchainKHR> swapchain_ = std::nullopt;
	VkSurfaceFormatKHR swapchain_image_format_ = {};
	VkPresentModeKHR swapchain_present_mode_ = {};
	uint32_t swap_chain_image_count_ = 0;
	swap_chain_support_details swapchain_details_ = {};
	VkExtent2D swapchain_extent_ = {};
	std::optional<descriptor_allocator> global_descriptor_allocator_;
	std::optional<VkDescriptorSet> draw_image_descriptors_;
	std::optional<VkDescriptorSetLayout> draw_image_descriptor_layout_;
	std::optional<VkDescriptorSetLayout> gpu_scene_data_descriptor_layout_;

	// textures
	allocated_image_result create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
	                                    bool mip_mapped = false) const;
	allocated_image_result create_image(const void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
	                                    bool mip_mapped = false);
	void destroy_image(const allocated_image& img) const;

	// immediate submits
	std::optional<VkFence> imm_fence_;
	std::optional<VkCommandBuffer> imm_command_buffer_;
	std::optional<VkCommandPool> imm_command_pool_;

	// test meshes
	std::vector<VkShaderEXT> shaders_;
	std::optional<VkPipelineLayout> shader_pl_;
	float model_rot_x_ = 0.0f;
	float model_rot_y_ = 0.0f;
	float model_rot_z_ = 0.0f;
	float model_x_ = 0.0f;
	float model_y_ = 0.0f;
	float model_z_ = -15.0f;
	float model_scale_ = 1.0f;
	bool toggle_wire_frame_ = false;
	int blend_mode_ = 0;

	std::vector<std::shared_ptr<mesh_asset>> test_meshes_;

	// test textures
	std::optional<allocated_image> white_image_ = std::nullopt;
	std::optional<allocated_image> black_image_ = std::nullopt;
	std::optional<allocated_image> grey_image_ = std::nullopt;
	std::optional<allocated_image> error_checkerboard_image_ = std::nullopt;

	std::optional<VkSampler> default_sampler_linear_ = std::nullopt;
	std::optional<VkSampler> default_sampler_nearest_ = std::nullopt;

	// ui
	std::optional<VkDescriptorPool> ui_pool_ = std::nullopt;

	size_t current_frame_ = 0;

	// main draw image
	std::optional<allocated_image> draw_image_;
	std::optional<allocated_image> depth_image_;
	VkExtent2D draw_extent_ = {};
	float render_scale_ = 1.f;

	std::optional <VkDescriptorSetLayout> single_image_descriptor_layout_;

	// swapchain images
	std::vector<VkImage> swap_chain_images_;
	std::vector<VkImageView> swap_chain_image_views_;

	std::vector<frame_data> frame_datas_;

	std::optional<VkDebugUtilsMessengerEXT> debug_messenger_ = std::nullopt;

	VkResult query_instance_layers();
	VkResult query_device_layers() const;
	VkResult query_instance_extensions();
	VkResult query_device_extensions();
	VkResult create_debug_callback();
	VkResult init_instance();
	VkResult init_surface(SDL_Window* window);
	VkResult init_physical_device();
	VkResult init_device();
	void init_allocator();
	VkResult init_presentation_queue();
	VkResult init_swap_chain(SDL_Window* window);
	VkResult create_swapchain(SDL_Window* window, VkSwapchainKHR old_swapchain);
	VkResult init_draw_image();
	VkResult init_descriptors();
	VkResult init_graphics();
	VkResult create_shader_objects(const std::vector<char>& vert, const std::vector<char>& frag);
	VkResult init_command_pool();
	VkResult init_command_buffers();
	VkResult init_sync_objects();
	VkResult init_commands();
	VkResult init_default_data();

	// Rendering
	static void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout,
	                             VkImageLayout new_layout);
	VkResult render_frame();
	VkResult immediate_submit(std::function<void(VkCommandBuffer cmd)>&& record_func) const;

	// Cmd
	void set_rendering_defaults(VkCommandBuffer cmd);
	void toggle_depth(VkCommandBuffer cmd, bool enable);
	void toggle_culling(VkCommandBuffer cmd, bool enable);
	void toggle_wire_frame(VkCommandBuffer cmd, bool enable, float line_width = 1.0);
	void set_view_port(VkCommandBuffer cmd, VkExtent2D extent);
	void disable_blending(VkCommandBuffer cmd);
	void enable_blending_additive(VkCommandBuffer cmd);
	void enable_blending_alpha_blend(VkCommandBuffer cmd);

	// Utils
	swap_chain_support_details query_swap_chain_support(VkPhysicalDevice device) const;
	static VkImageCreateInfo img_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent);
	static VkImageViewCreateInfo img_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);
	static VkRenderingAttachmentInfo attachment_info(VkImageView view, VkImageLayout layout);
	static VkRenderingAttachmentInfo depth_attachment_info(VkImageView view, VkImageLayout layout);
	static VkRenderingInfo rendering_info(VkExtent2D render_extent, const VkRenderingAttachmentInfo& color_attachment,
	                                      const std::optional<VkRenderingAttachmentInfo>& depth_attachment);
	static void blit_images(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size,
	                        VkExtent2D dst_size);
	allocated_buffer_result create_buffer(size_t alloc_size, VkBufferUsageFlags usage,
	                                      VmaMemoryUsage memory_usage) const;
	static VkDebugUtilsObjectNameInfoEXT add_name(VkObjectType object_type, uint64_t object_handle,
	                                              const char* p_object_name);

	// ui
	VkResult init_ui(SDL_Window* window);
	VkResult render_ui(VkCommandBuffer cmd, VkImageView target_image_view) const;

	// destructors
	void destroy_swapchain();
	void destroy_buffer(const allocated_buffer& buffer) const;
	void deinit_ui() const;
};
