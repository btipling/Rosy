#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include "../config/Config.h"

struct RhiInitResult {
	VkResult result;
	VkInstance instance;
	std::optional<VkPhysicalDevice> physicalDevice;
	std::optional<VkPhysicalDeviceProperties> physicalDeviceProperties;
	std::optional<VkPhysicalDeviceFeatures> supportedFeatures;
	std::optional<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProperties;
	std::optional<std::vector<VkQueueFamilyProperties>> queueFamilyProperties;
	std::uint32_t queueIndex = 0;
	std::uint32_t queueCount = 0;
	VkPhysicalDeviceFeatures requiredFeatures;
	std::optional<VkDevice> device;
};

RhiInitResult RhiInit(rosy_config::Config cfg);