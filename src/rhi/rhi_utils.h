#pragma once
#include <vector>
#include <fstream>

VkDebugUtilsMessengerCreateInfoEXT createDebugCallbackInfo();


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

std::vector<char> readFile(const std::string& filename);