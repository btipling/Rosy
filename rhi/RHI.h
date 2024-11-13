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
	std::optional<VkPhysicalDeviceFeatures> physicalDeviceFeatures;
	std::optional<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProperties;
	std::optional<std::vector<VkQueueFamilyProperties>> queueFamilyProperties;
};

RhiInitResult RhiInit(rosy_config::Config cfg);