#include "RHI.h"
#include <Windows.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <strsafe.h>
#include "../config/Config.h"
#include "../utils/utils.h"


RhiInitResult RhiInit(rosy_config::Config cfg) {

    uint32_t extensionCount;
    auto extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Rosy";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Rosy";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensionNames;

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, NULL, &instance);

    std::vector<VkPhysicalDevice> m_physicalDevices;
    std::optional<VkPhysicalDeviceProperties> physicalDeviceProperties = std::nullopt;

    if (result == VK_SUCCESS) {
        OutputDebugStringW(L"Vulkan instance created successfully!\n");
        uint32_t physicalDeviceCount = 0;
        result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
        if (result == VK_SUCCESS) {
            m_physicalDevices.resize(physicalDeviceCount);
            vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &m_physicalDevices[0]);
            for (const VkPhysicalDevice& device : m_physicalDevices) {
                VkPhysicalDeviceProperties deviceProperties;
                vkGetPhysicalDeviceProperties(device, &deviceProperties);
                if (deviceProperties.vendorID == cfg.device_vendor) {
                    physicalDeviceProperties = deviceProperties;
                }
            }
        }
    }
    else {
        OutputDebugStringW(L"Failed to create Vulkan instance!\n");
    }

    struct RhiInitResult res = RhiInitResult {
        .result = result,
        .instance = instance,
        .physicalDeviceProperties = physicalDeviceProperties,
    };
    return res;
}