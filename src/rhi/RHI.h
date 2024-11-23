#pragma once
#include "rhi_types.h"

#define MAX_FRAMES_IN_FLIGHT 2

class Rhi {
public:
	Rhi(rosy_config::Config cfg);
	VkResult init(SDL_Window* window);
	VkResult drawUI();
	VkResult drawFrame();
	void debug();
	~Rhi();
private:
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
	VkFormat m_swapChainImageFormat = {};
	VkExtent2D m_swapChainExtent = {};
	std::optional<VkCommandPool> m_commandPool = std::nullopt;

	// hello triangle stuff
	std::vector<VkShaderEXT> m_shaders;
	std::optional<VkPipelineLayout> m_shaderPL;
	float m_triangle_rot = 0.5;

	// ui
	std::optional<VkDescriptorPool> m_uiPool = std::nullopt;

	size_t m_currentFrame = 0;

	// main draw image
	std::optional<AllocatedImage> m_drawImage;
	VkExtent2D m_drawExtent = {};

	// swapchain images
	std::vector<VkImage> m_swapChainImages;
	std::vector<VkImageView> m_swapChainImageViews;

	// per frame data
	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFence;

	std::optional <VkDebugUtilsMessengerEXT> m_debugMessenger = std::nullopt;

	VkResult queryInstanceLayers();
	VkResult queryDeviceLayers();
	VkResult queryInstanceExtensions();
	VkResult queryDeviceExtensions();
	VkResult createDebugCallback();
	VkResult initInstance();
	VkResult initSurface(SDL_Window* window);
	VkResult initPhysicalDevice();
	VkResult initDevice();
	void initAllocator();
	VkResult initPresentationQueue();
	VkResult initSwapChain(SDL_Window* window);
	VkResult initDrawImage();
	VkResult initImageViews();
	VkResult initGraphics();
	VkResult createShaderObjects(const std::vector<char>& vert, const std::vector<char>& frag);
	VkResult initCommandPool();
	VkResult initCommandBuffers();
	VkResult initSyncObjects();
	
	void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
	VkResult renderFrame();

	// Utils
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkImageCreateInfo imgCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VkImageViewCreateInfo imgViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
	VkRenderingAttachmentInfo attachmentInfo(VkImageView view, VkImageLayout layout);
	VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo colorAttachment);
	void blitImages(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
	AllocatedBufferResult createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	// Buffer read write
	GPUMeshBuffersResult uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);


	// ui
	VkResult initUI(SDL_Window* window);
	VkResult renderUI(VkCommandBuffer cmd, VkImageView targetImageView);
	void deinitUI();

};