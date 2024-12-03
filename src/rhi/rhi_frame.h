#pragma once
#include "../Rosy.h"
#include "rhi_types.h"
#include "rhi_descriptor.h"


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
	enum class result : std::uint8_t { ok, error };

	struct rhi
	{
		VkDevice device;
		VmaAllocator allocator;
		std::optional<frame_data> frame_data = std::nullopt;
		std::optional<rhi_data*> data = std::nullopt;
		VkExtent2D frame_extent;
		std::optional <descriptor_allocator> global_descriptor_allocator;
	};

	struct ctx
	{
		rhi rhi;
	};
}