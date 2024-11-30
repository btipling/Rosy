#pragma once

#include "resource.h"
#include <Windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
#include <optional>
#include <vector>
#include <span>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "config/Config.h"
#include "utils/utils.h"
#include <fstream>
#include <array>
#include <deque>

namespace rh
{
	enum class result : std::uint8_t { ok, error };

	struct rhi
	{
		std::shared_ptr <VkDevice> device;
		std::optional<std::shared_ptr<VkCommandBuffer>> cmd;
	};

	struct ctx
	{
		std::shared_ptr<rhi> rhi;
	};
}