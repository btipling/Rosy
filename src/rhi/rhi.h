#pragma once
#include "../Rosy.h"
#include "../loader/loader.h"
#include "rhi_descriptor.h"

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
	void debug();
	~rhi();

private:
	bool m_deinited_ = false;
	rosy_config::Config m_cfg_;
	std::vector<const char*> m_instance_layer_properties_;
	std::vector<const char*> m_device_layer_properties_;
	std::vector<const char*> m_instance_extensions_;
	std::vector<const char*> m_device_device_extensions_;
	std::optional<VkInstance> m_instance_ = std::nullopt;
	std::optional<VkPhysicalDevice> m_physical_device_ = std::nullopt;
	std::optional<VkPhysicalDeviceProperties> m_physical_device_properties_ = std::nullopt;
	std::optional<VkPhysicalDeviceFeatures> m_supported_features_ = std::nullopt;
	std::optional<VkPhysicalDeviceMemoryProperties> m_physical_device_memory_properties_ = std::nullopt;
	std::optional<std::vector<VkQueueFamilyProperties>> m_queue_family_properties_ = std::nullopt;
	std::uint32_t m_queue_index_ = 0;
	std::uint32_t m_queue_count_ = 0;
	std::vector<float> m_queue_priorities_;
	VkPhysicalDeviceFeatures m_required_features_;
	std::optional<VkDevice> m_device_ = std::nullopt;
	std::optional<VmaAllocator> m_allocator_ = std::nullopt;
	std::optional<VkSurfaceKHR> m_surface_ = std::nullopt;
	std::optional<VkQueue> m_present_queue_ = std::nullopt;
	std::optional<VkSwapchainKHR> m_swapchain_ = std::nullopt;
	VkSurfaceFormatKHR m_swapchain_image_format_ = {};
	VkPresentModeKHR m_swapchain_present_mode_ = {};
	uint32_t m_swap_chain_image_count_ = 0;
	swap_chain_support_details m_swapchain_details_ = {};
	VkExtent2D m_swapchain_extent_ = {};
	std::optional<VkCommandPool> m_command_pool_ = std::nullopt;
	std::optional<descriptor_allocator> m_global_descriptor_allocator_;
	std::optional<VkDescriptorSet> m_draw_image_descriptors_;
	std::optional<VkDescriptorSetLayout> m_draw_image_descriptor_layout_;

	// immediate submits
	std::optional<VkFence> m_imm_fence_;
	std::optional<VkCommandBuffer> m_imm_command_buffer_;
	std::optional<VkCommandPool> m_imm_command_pool_;

	// test meshes
	std::vector<VkShaderEXT> m_shaders_;
	std::optional<VkPipelineLayout> m_shader_pl_;
	float m_model_rot_x_ = 0.0f;
	float m_model_rot_y_ = 0.0f;
	float m_model_rot_z_ = 0.0f;
	float m_model_x_ = 0.0f;
	float m_model_y_ = 0.0f;
	float m_model_z_ = -15.0f;
	float m_model_scale_ = 1.0f;
	bool m_toggle_wire_frame_ = false;
	int m_blend_mode_ = 0;

	std::vector<std::shared_ptr<mesh_asset>> m_test_meshes_;

	// ui
	std::optional<VkDescriptorPool> m_ui_pool_ = std::nullopt;

	size_t m_current_frame_ = 0;

	// main draw image
	std::optional<allocated_image> m_draw_image_;
	std::optional<allocated_image> m_depth_image_;
	VkExtent2D m_draw_extent_ = {};
	float m_render_scale_ = 1.f;

	// swapchain images
	std::vector<VkImage> m_swap_chain_images_;
	std::vector<VkImageView> m_swap_chain_image_views_;

	// per frame data
	std::vector<VkCommandBuffer> m_command_buffers_;
	std::vector<VkSemaphore> m_image_available_semaphores_;
	std::vector<VkSemaphore> m_render_finished_semaphores_;
	std::vector<VkFence> m_in_flight_fence_;

	std::optional<VkDebugUtilsMessengerEXT> m_debug_messenger_ = std::nullopt;

	VkResult query_instance_layers();
	VkResult query_device_layers();
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
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout,
	                      VkImageAspectFlags aspectMask);
	VkResult render_frame();
	VkResult immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	// Cmd
	void set_rendering_defaults(VkCommandBuffer cmd);
	void toggle_depth(VkCommandBuffer cmd, bool enable);
	void toggle_culling(VkCommandBuffer cmd, bool enable);
	void toggle_wire_frame(VkCommandBuffer cmd, bool enable);
	void set_view_port(VkCommandBuffer cmd, VkExtent2D extent);
	void disable_blending(VkCommandBuffer cmd);
	void enable_blending_additive(VkCommandBuffer cmd);
	void enable_blending_alpha_blend(VkCommandBuffer cmd);

	// Utils
	swap_chain_support_details querySwapChainSupport(VkPhysicalDevice device);
	VkImageCreateInfo imgCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VkImageViewCreateInfo imgViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
	VkRenderingAttachmentInfo attachmentInfo(VkImageView view, VkImageLayout layout);
	VkRenderingAttachmentInfo depthAttachmentInfo(VkImageView view, VkImageLayout layout);
	VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo colorAttachment,
	                              std::optional<VkRenderingAttachmentInfo> depthAttachment);
	void blitImages(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
	allocated_buffer_result createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	VkDebugUtilsObjectNameInfoEXT addName(VkObjectType objectType, uint64_t objectHandle, const char* pObjectName);

	// ui
	VkResult initUI(SDL_Window* window);
	VkResult renderUI(VkCommandBuffer cmd, VkImageView targetImageView);

	// destructors
	void destroySwapchain();
	void destroyBuffer(const allocated_buffer& buffer);
	void deinitUI();
};
