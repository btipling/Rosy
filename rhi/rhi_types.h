#pragma once
#include <Windows.h>
#include <vulkan/vulkan.h>
#include "vma/vk_mem_alloc.h"
#include <optional>
#include <vector>
#include "../config/Config.h"
#include "../utils/utils.h"
#include "rhi_debug.h"

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
             fmt::print("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)