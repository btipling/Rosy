#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include "../config/Config.h"

struct RhiInitResult {
	VkResult result;
	VkInstance instance;
	std::optional<VkPhysicalDeviceProperties> physicalDeviceProperties;
	std::optional<VkPhysicalDeviceFeatures> physicalDeviceFeatures;

};

RhiInitResult RhiInit(rosy_config::Config cfg);