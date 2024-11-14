#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include "../config/Config.h"


class Rhi {
public:
	void init(rosy_config::Config cfg);
	void debug();
	~Rhi();
private:
	std::optional<VkInstance> m_instance = std::nullopt;
	std::optional<VkPhysicalDevice> m_physicalDevice = std::nullopt;
	std::optional<VkPhysicalDeviceProperties> m_physicalDeviceProperties = std::nullopt;
	std::optional<VkPhysicalDeviceFeatures> m_supportedFeatures = std::nullopt;
	std::optional<VkPhysicalDeviceMemoryProperties> m_physicalDeviceMemoryProperties = std::nullopt;
	std::optional<std::vector<VkQueueFamilyProperties>> m_queueFamilyProperties = std::nullopt;
	std::uint32_t m_queueIndex = 0;
	std::uint32_t m_queueCount = 0;
	VkPhysicalDeviceFeatures m_requiredFeatures;
	std::optional<VkDevice> m_device = std::nullopt;

	VkResult initInstance(rosy_config::Config cfg);
	VkResult initPhysicalDevice(rosy_config::Config cfg);
	VkResult initDevice(rosy_config::Config cfg);

};