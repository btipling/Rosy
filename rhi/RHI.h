#pragma once
#include "rhi_types.h"


class Rhi {
public:
	Rhi(rosy_config::Config cfg);
	VkResult init(SDL_Window* window);
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
	std::vector<VkImage> swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	std::vector<VkImageView> m_swapChainImageViews;

	std::optional <VkDebugUtilsMessengerEXT> m_debugMessenger = std::nullopt;

	VkResult queryInstanceLayers();
	VkResult queryDeviceLayers();
	VkResult queryInstanceExtensions();
	VkResult queryDeviceExtensions();
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkResult createDebugCallback();
	VkResult initInstance();
	VkResult initSurface(SDL_Window* window);
	VkResult initPhysicalDevice();
	VkResult initDevice();
	void initAllocator();
	VkResult initPresentationQueue();
	VkResult initSwapChain(SDL_Window* window);
	VkResult initImageViews();
	VkResult initGraphics();
	VkShaderEXT createShaderObject(const std::vector<char>& code);
};