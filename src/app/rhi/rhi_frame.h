#pragma once
#include "../../../Rosy.h"
#include "rhi_types.h"
#include "rhi_descriptor.h"
#include "rhi_shader.h"
#include "rhi_debug.h"
#include "rhi_mesh.h"
#include "SDL3/SDL_events.h"


struct frame_data
{
	std::optional<VkCommandBuffer> command_buffer;
	std::optional<VkSemaphore> image_available_semaphore;
	std::optional<VkSemaphore> render_finished_semaphore;
	std::optional<VkFence> in_flight_fence;
	std::optional<VkCommandPool> command_pool;
	std::optional<descriptor_allocator_growable> frame_descriptors;

	std::optional<allocated_buffer> gpu_scene_buffer = std::nullopt;
};

namespace rh
{
	struct rhi
	{
		VkDevice device{};
		VmaAllocator allocator{};
		std::optional<frame_data> frame_data = std::nullopt;
		std::optional<rhi_data*> data = std::nullopt;
		VkExtent2D frame_extent{};
		VkExtent2D shadow_map_extent{};
		std::optional <descriptor_allocator_growable> descriptor_allocator;
	};

	struct ctx
	{
		rhi rhi{};
		const SDL_Event* sdl_event{};
		bool mouse_enabled = true;
		double current_time = 0.f;
	};
}