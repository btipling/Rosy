#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include "../config/Config.h"

struct RhiInitResult {
	VkResult result;
	VkInstance instance;
	std::optional<VkPhysicalDeviceProperties> physicalDeviceProperties;

};

RhiInitResult RhiInit(rosy_config::Config cfg);