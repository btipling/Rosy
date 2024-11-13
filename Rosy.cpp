// Rosy.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Rosy.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vector>
#include "rhi/RHI.h"
#include "config/Config.h"
#include "utils/utils.h"

int main(int argc, char* argv[])
{
    const int WIDTH = 640;
    const int HEIGHT = 480;
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Rosy", WIDTH, HEIGHT, SDL_WINDOW_VULKAN);
    renderer = SDL_CreateRenderer(window, NULL);

    rosy_config::Config cfg = {};
    rosy_config::debug();

    RhiInitResult initResult = RhiInit(cfg);

    if (initResult.physicalDeviceProperties.has_value()) {
        VkPhysicalDeviceProperties deviceProperties = initResult.physicalDeviceProperties.value();
        VkPhysicalDeviceFeatures deviceFeatures = initResult.physicalDeviceFeatures.value();
        VkPhysicalDeviceMemoryProperties deviceMemProps = initResult.physicalDeviceMemoryProperties.value();
        std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData = initResult.queueFamilyProperties.value();
        rosy_utils::DebugPrintA("result device property vendor %s \n", deviceProperties.deviceName);
        rosy_utils::DebugPrintA("result: vendor: %u \n", deviceProperties.vendorID);

        rosy_utils::DebugPrintA("has multiDrawIndirect? %d \n", deviceFeatures.multiDrawIndirect);
        for (int i = 0; i < deviceMemProps.memoryHeapCount; i++) {
            rosy_utils::DebugPrintA("memory size: %d\n", deviceMemProps.memoryHeaps[i].size);
            rosy_utils::DebugPrintA("memory flags: %d\n", deviceMemProps.memoryHeaps[i].flags);
        }
        for (VkQueueFamilyProperties qfmp : queueFamilyPropertiesData) {
            rosy_utils::DebugPrintA("queue count: %d\n", qfmp.queueCount);
            if (qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT)) {
                rosy_utils::DebugPrintA("VkQueueFamilyProperties got all the things\n");
            }
            else {
                rosy_utils::DebugPrintA("VkQueueFamilyProperties missing stuff\n");
            }
        }
    }
    else {
        rosy_utils::DebugPrintA("no config!");
    }


    bool should_run = true;
    while (should_run) {
        SDL_Event windowEvent;
        while (SDL_PollEvent(&windowEvent)) {
            if (windowEvent.type == SDL_EVENT_QUIT) {
                should_run = false;
                break;
            }
            else {
                // do nothing
            }
        }
    }

    if (initResult.result == VK_SUCCESS) {
        vkDestroyInstance(initResult.instance, NULL);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
