#include "RHI.h"
#include <Windows.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

RhiInitResult RhiInit() {

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

    if (result == VK_SUCCESS) {
        OutputDebugStringW(L"Vulkan instance created successfully!\n");
    }
    else {
        OutputDebugStringW(L"Failed to create Vulkan instance!\n");
    }

    struct RhiInitResult res = RhiInitResult {
        .result = result,
        .instance = instance,
    };
    return res;
}