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
	std::optional<VkCommandBuffer> shadow_pass_command_buffer = std::nullopt;
	std::optional<VkCommandBuffer> render_command_buffer = std::nullopt;
	std::optional<VkSemaphore> image_available_semaphore = std::nullopt;
	std::optional<VkSemaphore> shadow_pass_semaphore = std::nullopt;
	std::optional<VkSemaphore> render_finished_semaphore = std::nullopt;
	std::optional<VkFence> shadow_pass_fence = std::nullopt;
	std::optional<VkFence> in_flight_fence = std::nullopt;
	std::optional<VkCommandPool> command_pool = std::nullopt;
};

namespace rh
{
	struct rhi
	{
		VkDevice device{};
		VmaAllocator allocator{};
		std::optional<VkCommandBuffer> shadow_pass_command_buffer = std::nullopt;
		std::optional<VkCommandBuffer> render_command_buffer = std::nullopt;
		std::optional<rhi_data*> data = std::nullopt;
		std::optional<descriptor_sets_manager*> descriptor_sets = std::nullopt;
		VkExtent2D frame_extent{};
		VkExtent2D shadow_map_extent{};
	};

	struct ctx
	{
		rhi rhi{};
		const SDL_Event* sdl_event{};
		bool mouse_enabled = true;
		double current_time = 0.f;
	};
}