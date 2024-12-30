#pragma once
#include "rhi_frame.h"

#define MAX_FRAMES_IN_FLIGHT 2  // NOLINT(modernize-macro-to-enum)

class rhi
{
public:
	std::expected<rh::ctx, VkResult> current_render_ctx(const SDL_Event* event) const;
	std::expected<rh::ctx, VkResult> current_shadow_pass_ctx(const SDL_Event* event) const;
	std::optional<VkDevice> opt_device = std::nullopt;
	std::optional<VmaAllocator> opt_allocator = std::nullopt;
	std::optional<std::unique_ptr<rhi_data>> buffer;
	VkExtent2D swapchain_extent = {};
	std::optional<ktxVulkanDeviceInfo> vdi = std::nullopt;
	rosy_config::config* app_cfg;
	std::optional<std::unique_ptr<descriptor_sets_manager>> descriptor_sets = std::nullopt;

	explicit rhi(rosy_config::config* cfg);
	VkResult init(SDL_Window* window);
	VkResult resize_swapchain(SDL_Window* window);
	void deinit();
	VkResult begin_frame();
	VkResult init_shadow_pass();
	VkResult begin_shadow_pass(int pass_number) const;
	VkResult end_shadow_pass() const;
	VkResult render_pass();
	VkResult end_frame();
	VkResult draw_ui();
	// Buffer read write
	VkResult immediate_submit(std::function<void(VkCommandBuffer cmd)>&& record_func) const;
	// Rendering
	static void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout,
		VkImageLayout new_layout);
	static void transition_shadow_map_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout,
		VkImageLayout new_layout, VkPipelineStageFlags2 src_stage_flags, VkPipelineStageFlags2 dst_stage_flags);
	void debug() const;
	~rhi();
	rhi(const rhi&) = delete;
	rhi& operator=(const rhi&) = delete;
	rhi(rhi&&) noexcept = default;
	rhi& operator=(rhi&&) noexcept = default;

private:
	bool deinited_ = false;
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
	std::optional<VkSurfaceKHR> surface_ = std::nullopt;
	std::optional<VkQueue> present_queue_ = std::nullopt;
	std::optional<VkSwapchainKHR> swapchain_ = std::nullopt;
	VkSurfaceFormatKHR swapchain_image_format_ = {};
	VkPresentModeKHR swapchain_present_mode_ = {};
	uint32_t swap_chain_image_count_ = 0;
	swap_chain_support_details swapchain_details_ = {};

	// immediate submits
	std::optional<VkFence> imm_fence_;
	std::optional<VkCommandBuffer> imm_command_buffer_;
	std::optional<VkCommandPool> imm_command_pool_;

	// ui
	std::optional<VkDescriptorPool> ui_pool_ = std::nullopt;
	size_t current_viewed_shadow_map_ = 0;

	size_t current_frame_ = 0;
	uint32_t current_swapchain_image_index_ = 0;

	// main draw image
	std::optional<allocated_image> draw_image_;
	std::optional<allocated_image> depth_image_;
	std::optional<allocated_csm> shadow_map_image_;
	VkExtent2D draw_extent_ = {};
	float render_scale_ = 1.f;

	// swapchain images
	std::vector<VkImage> swap_chain_images_;
	std::vector<VkImageView> swap_chain_image_views_;

	std::vector<frame_data> frame_datas_;

	std::optional<VkDebugUtilsMessengerEXT> debug_messenger_ = std::nullopt;

	VkResult query_instance_layers();
	[[nodiscard]] VkResult query_device_layers() const;
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
	VkResult init_command_pool();
	VkResult init_command_buffers();
	VkResult init_sync_objects();
	VkResult init_commands();
	VkResult init_data();
	VkResult init_ktx();
	VkResult init_csm_image();

	// Utils
	swap_chain_support_details query_swap_chain_support(VkPhysicalDevice device) const;

	// ui
	VkResult init_ui(SDL_Window* window);
	VkResult render_ui(VkCommandBuffer cmd, VkImageView target_image_view) const;

	// destructors
	void destroy_swapchain();
	void deinit_ui() const;
};
