#pragma once
#include <vector>
#include <fstream>
#include <glm/glm.hpp>

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

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct AllocatedBufferResult {
    VkResult result;
    AllocatedBuffer buffer;
};

struct Vertex {
    glm::vec4 position;
    glm::vec4 textureCoordinates;
    glm::vec4 normal;
    glm::vec4 color;
};

struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct GPUMeshBuffersResult {
    VkResult result;
    GPUMeshBuffers buffers;
};

struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};


std::vector<char> readFile(const std::string& filename);