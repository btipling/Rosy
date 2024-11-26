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
#include <vector>
#include <fstream>
#include <glm/glm.hpp>
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

struct frame_data
{
    VkCommandBuffer command_buffers;
    VkSemaphore image_available_semaphores;
    VkSemaphore render_finished_semaphores;
    VkFence in_flight_fence;
};

struct swap_chain_support_details {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct allocated_image {
    VkImage image;
    VkImageView image_view;
    VmaAllocation allocation;
    VkExtent3D image_extent;
    VkFormat image_format;
};

struct allocated_buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct allocated_buffer_result {
    VkResult result;
    allocated_buffer buffer;
};

struct vertex {
    glm::vec4 position;
    glm::vec4 texture_coordinates;
    glm::vec4 normal;
    glm::vec4 color;
};

struct gpu_mesh_buffers {
    allocated_buffer index_buffer;
    allocated_buffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
};

struct gpu_mesh_buffers_result {
    VkResult result;
    gpu_mesh_buffers buffers;
};

struct gpu_draw_push_constants {
    glm::mat4 world_matrix;
    VkDeviceAddress vertex_buffer;
};

struct geo_surface {
    uint32_t start_index;
    uint32_t count;
};

struct mesh_asset {
    std::string name;

    std::vector<geo_surface> surfaces;
    gpu_mesh_buffers mesh_buffers;
};
