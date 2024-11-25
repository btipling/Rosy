#pragma once
#include "../Rosy.h"
#include "../loader/loader.h"
#include "rhi_descriptor.h"

#define MAX_FRAMES_IN_FLIGHT 2

class Rhi
{
public:
	Rhi(rosy_config::Config cfg);
	VkResult init(SDL_Window* window);
	VkResult resizeSwapchain(SDL_Window* window);
	void deinit();
	VkResult drawUI();
	VkResult drawFrame();
	// Buffer read write
	GPUMeshBuffersResult uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	void debug();
	~Rhi();

private:
	bool m_deinited = false;
	rosy_config::Config m_cfg;
	std::vector<const char*> m_instanceLayerProperties;
	std::vector<const char*> m_deviceLayerProperties;
	std::vector<const char*> m_instanceExtensions;
	std::vector<const char*> m_deviceDeviceExtensions;
	std::optional<VkInstance> m_instance = std::nullopt;
	std::optional<VkPhysicalDevice> m_physicalDevice = std::nullopt;
	std::optional<VkPhysicalDeviceProperties> m_physicalDeviceProperties = std::nullopt;
	std::optional<VkPhysicalDeviceFeatures> m_supportedFeatures = std::nullopt;
	std::optional<VkPhysicalDeviceMemoryProperties> m_physicalDeviceMemoryProperties = std::nullopt;
	std::optional<std::vector<VkQueueFamilyProperties>> m_queueFamilyProperties = std::nullopt;
	std::uint32_t m_queueIndex = 0;
	std::uint32_t m_queueCount = 0;
	std::vector<float> m_queuePriorities;
	VkPhysicalDeviceFeatures m_requiredFeatures;
	std::optional<VkDevice> m_device = std::nullopt;
	std::optional<VmaAllocator> m_allocator = std::nullopt;
	std::optional<VkSurfaceKHR> m_surface = std::nullopt;
	std::optional<VkQueue> m_presentQueue = std::nullopt;
	std::optional<VkSwapchainKHR> m_swapchain = std::nullopt;
	VkSurfaceFormatKHR m_swapchainImageFormat = {};
	VkPresentModeKHR m_swapchainPresentMode = {};
	uint32_t m_swapChainImageCount = 0;
	SwapChainSupportDetails m_swapchainDetails = {};
	VkExtent2D m_swapchainExtent = {};
	std::optional<VkCommandPool> m_commandPool = std::nullopt;
	std::optional<DescriptorAllocator> m_globalDescriptorAllocator;
	std::optional<VkDescriptorSet> m_drawImageDescriptors;
	std::optional<VkDescriptorSetLayout> m_drawImageDescriptorLayout;

	// immediate submits
	std::optional<VkFence> m_immFence;
	std::optional<VkCommandBuffer> m_immCommandBuffer;
	std::optional<VkCommandPool> m_immCommandPool;

	// test meshes
	std::vector<VkShaderEXT> m_shaders;
	std::optional<VkPipelineLayout> m_shaderPL;
	float m_model_rot_x = 0.0f;
	float m_model_rot_y = 0.0f;
	float m_model_rot_z = 0.0f;
	float m_model_x = 0.0f;
	float m_model_y = 0.0f;
	float m_model_z = -15.0f;
	float m_model_scale = 1.0f;
	bool m_toggleWireFrame = false;
	int m_blendMode = 0;

	std::vector<std::shared_ptr<MeshAsset>> m_testMeshes;

	// ui
	std::optional<VkDescriptorPool> m_uiPool = std::nullopt;

	size_t m_currentFrame = 0;

	// main draw image
	std::optional<AllocatedImage> m_drawImage;
	std::optional<AllocatedImage> m_depthImage;
	VkExtent2D m_drawExtent = {};
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
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkImageCreateInfo imgCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VkImageViewCreateInfo imgViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
	VkRenderingAttachmentInfo attachmentInfo(VkImageView view, VkImageLayout layout);
	VkRenderingAttachmentInfo depthAttachmentInfo(VkImageView view, VkImageLayout layout);
	VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo colorAttachment,
	                              std::optional<VkRenderingAttachmentInfo> depthAttachment);
	void blitImages(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
	AllocatedBufferResult createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	VkDebugUtilsObjectNameInfoEXT addName(VkObjectType objectType, uint64_t objectHandle, const char* pObjectName);

	// ui
	VkResult initUI(SDL_Window* window);
	VkResult renderUI(VkCommandBuffer cmd, VkImageView targetImageView);

	// destructors
	void destroySwapchain();
	void destroyBuffer(const AllocatedBuffer& buffer);
	void deinitUI();
};
