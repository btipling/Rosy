#pragma once
#include <vulkan/vulkan.h>

struct RhiInitResult {
	VkResult result;
	VkInstance instance;
};

RhiInitResult RhiInit();