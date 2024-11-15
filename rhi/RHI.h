#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include "../config/Config.h"


class Rhi {
public:
	Rhi(rosy_config::Config cfg);
	void init();
	void debug();
	~Rhi();
private:
	rosy_config::Config m_cfg;
	std::vector<const char*> m_instanceLayerProperties;
	std::vector<const char*> m_deviceLayerProperties;
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

	VkResult queryInstanceLayers();
	VkResult queryDeviceLayers();
	VkResult initInstance();
	VkResult initPhysicalDevice();
	VkResult initDevice();
};