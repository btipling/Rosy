#pragma once
#include <Windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
#include <optional>
#include <vector>
#include "../config/Config.h"
#include "../utils/utils.h"
#include "rhi_debug.h"
#include "rhi_spirv.h"


struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};
