#pragma once
#include <vector>
#include <fstream>

std::vector<char> readFile(const std::string& filename);


VkResult CreateShadersEXT(
    VkInstance instance,
    VkDevice device,
    uint32_t createInfoCount,
    const VkShaderCreateInfoEXT* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkShaderEXT* pShaders);