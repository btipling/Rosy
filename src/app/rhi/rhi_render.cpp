#include "imgui.h"
#include "rhi.h"


void rhi::transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageLayout current_layout,
	const VkImageLayout new_layout)
{
	const VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
		? VK_IMAGE_ASPECT_DEPTH_BIT
		: VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageSubresourceRange subresource_range = rhi_helpers::create_img_subresource_range(aspect_mask);
	subresource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageMemoryBarrier2 image_barrier = {};
	image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	image_barrier.pNext = nullptr;
	image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	image_barrier.oldLayout = current_layout;
	image_barrier.newLayout = new_layout;
	image_barrier.subresourceRange = subresource_range;
	image_barrier.image = image;

	VkDependencyInfo dependency_info{};
	dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency_info.pNext = nullptr;

	dependency_info.imageMemoryBarrierCount = 1;
	dependency_info.pImageMemoryBarriers = &image_barrier;

	vkCmdPipelineBarrier2(cmd, &dependency_info);
}

VkResult rhi::draw_ui()
{
	// Draw some FPS statistics or something
	return VK_SUCCESS;
}

std::expected<rh::ctx, VkResult> rhi::current_frame_data(const SDL_Event* event)
{
	if (frame_datas_.size() == 0) return std::unexpected(VK_ERROR_UNKNOWN);
	const VkExtent2D shadow_map_extent = {
		.width = shadow_map_image_.value().image_extent.width,
		.height = shadow_map_image_.value().image_extent.height,
	};
	rh::rhi rhi_ctx = {
		.device = opt_device.value(),
		.allocator = opt_allocator.value(),
		.frame_extent = swapchain_extent,
		.shadow_map_extent = shadow_map_extent,
	};
	if (frame_datas_.size() > 0) {
		rhi_ctx.frame_data = frame_datas_[current_frame_];
	}
	if (buffer.has_value())
	{
		rhi_ctx.data = buffer.value().get();
	}
	if (global_descriptor_allocator_.has_value())
	{
		rhi_ctx.descriptor_allocator = scene_descriptor_allocator;
	}
	SDL_Time ticks = 0;
	if (!SDL_GetCurrentTime(&ticks))
	{
		rosy_utils::debug_print_a("failed to get current ticks!\n");
	}
	const rh::ctx ctx = {
		.rhi = rhi_ctx,
		.sdl_event = event,
		.current_time = static_cast<double>(ticks),
	};
	return ctx;
}

VkResult rhi::begin_frame()
{
	auto [opt_command_buffers, opt_image_available_semaphores, opt_render_finished_semaphores, opt_in_flight_fence,
		opt_command_pool, opt_frame_descriptors, opt_gpu_scene_buffer] = frame_datas_[current_frame_];
	{
		if (!opt_command_buffers.has_value()) return VK_NOT_READY;
		if (!opt_image_available_semaphores.has_value()) return VK_NOT_READY;
		if (!opt_render_finished_semaphores.has_value()) return VK_NOT_READY;
		if (!opt_in_flight_fence.has_value()) return VK_NOT_READY;
		if (!opt_command_pool.has_value()) return VK_NOT_READY;
	}

	VkCommandBuffer cmd = opt_command_buffers.value();
	VkSemaphore image_available = opt_image_available_semaphores.value();
	VkFence fence = opt_in_flight_fence.value();
	descriptor_allocator_growable frame_descriptors = opt_frame_descriptors.value();

	VkResult result;
	VkDevice device = opt_device.value();

	result = vkWaitForFences(device, 1, &fence, true, 1000000000);
	if (result != VK_SUCCESS) return result;
	frame_descriptors.clear_pools(device);
	if (opt_gpu_scene_buffer.has_value()) buffer.value()->destroy_buffer(opt_gpu_scene_buffer.value());
	frame_datas_[current_frame_].gpu_scene_buffer = std::nullopt;

	uint32_t image_index;
	// vkAcquireNextImageKHR will signal the imageAvailable semaphore which the submit queue call will wait for below.
	vkAcquireNextImageKHR(opt_device.value(), swapchain_.value(), UINT64_MAX, image_available, VK_NULL_HANDLE,
		&image_index);
	VkImage image = swap_chain_images_[image_index];
	VkImageView image_view = swap_chain_image_views_[image_index];
	current_swapchain_image_index_ = image_index;

	allocated_image draw_image = draw_image_.value();
	draw_extent_.width = std::min(swapchain_extent.width, draw_image.image_extent.width) * render_scale_;
	draw_extent_.height = std::min(swapchain_extent.height, draw_image.image_extent.height) * render_scale_;

	vkResetFences(device, 1, &fence);

	{
		// Start recording commands. This records commands that aren't actually submitted to a queue to do anything with until 
		// 1. The submit queue call has been made but also
		// 2. The wait semaphore given to the submit queue has been signaled, which
		// 3. Doesn't happen until the image we requested with vkAcquireNextImageKHR has been made available
		result = vkResetCommandBuffer(cmd, 0);
		if (result != VK_SUCCESS) return result;

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		result = vkBeginCommandBuffer(cmd, &begin_info);
		if (result != VK_SUCCESS) return result;
	}
	{
		//allocate a new uniform data for the scene data
		auto [result, created_buffer] = buffer.value()->create_buffer("gpu_scene_data", sizeof(gpu_scene_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_AUTO);
		if (result != VK_SUCCESS) return result;
		allocated_buffer gpu_scene_buffer = created_buffer;
		frame_datas_[current_frame_].gpu_scene_buffer = gpu_scene_buffer;
	}
	return VK_SUCCESS;
}


VkResult rhi::shadow_pass()
{
	auto [opt_command_buffers, opt_image_available_semaphores, opt_render_finished_semaphores, opt_in_flight_fence,
		opt_command_pool, opt_frame_descriptors, opt_gpu_scene_buffer] = frame_datas_[current_frame_];
	{
		if (!opt_command_buffers.has_value()) return VK_NOT_READY;
		if (!opt_image_available_semaphores.has_value()) return VK_NOT_READY;
		if (!opt_render_finished_semaphores.has_value()) return VK_NOT_READY;
		if (!opt_in_flight_fence.has_value()) return VK_NOT_READY;
		if (!opt_command_pool.has_value()) return VK_NOT_READY;
	}

	VkCommandBuffer cmd = opt_command_buffers.value();
	descriptor_allocator_growable frame_descriptors = opt_frame_descriptors.value();
	allocated_image shadow_map_image = shadow_map_image_.value();

	const VkExtent2D shadow_map_extent = {
		.width = shadow_map_image.image_extent.width,
		.height = shadow_map_image.image_extent.height,
	};
	transition_image(cmd, shadow_map_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	{
		VkRenderingAttachmentInfo depth_attachment = rhi_helpers::depth_attachment_info(shadow_map_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		VkRenderingInfo render_info = rhi_helpers::shadow_map_rendering_info(shadow_map_extent, depth_attachment);
		// begin shadow pass
		vkCmdBeginRendering(cmd, &render_info);
	}
	return VK_SUCCESS;
}


VkResult rhi::render_pass()
{
	allocated_image draw_image = draw_image_.value();
	allocated_image depth_image = depth_image_.value();
	auto [opt_command_buffers, opt_image_available_semaphores, opt_render_finished_semaphores, opt_in_flight_fence,
		opt_command_pool, opt_frame_descriptors, opt_gpu_scene_buffer] = frame_datas_[current_frame_];
	{
		if (!opt_command_buffers.has_value()) return VK_NOT_READY;
		if (!opt_image_available_semaphores.has_value()) return VK_NOT_READY;
		if (!opt_render_finished_semaphores.has_value()) return VK_NOT_READY;
		if (!opt_in_flight_fence.has_value()) return VK_NOT_READY;
		if (!opt_command_pool.has_value()) return VK_NOT_READY;
	}

	VkCommandBuffer cmd = opt_command_buffers.value();
	descriptor_allocator_growable frame_descriptors = opt_frame_descriptors.value();


	// end shadow pass
	vkCmdEndRendering(cmd);

	{
		// Clear image. This transition means that all the commands recorded before now happen before
		// any calls after. The calls themselves before this may have executed in any order up until this point.
		transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// vkCmdClearColorImage is guaranteed to happen after previous calls.
		VkClearColorValue clear_value;
		clear_value = { {0.0f, 0.05f, 0.1f, 1.0f} };
		VkImageSubresourceRange subresource_range = rhi_helpers::create_img_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &subresource_range);
	}
	{
		// Start dynamic render pass, again this sets a barrier between vkCmdClearColorImage and what happens after
		transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		transition_image(cmd, depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		//  and the subsequent happening between vkCmdBeginRendering and vkCmdEndRendering happen after this, but may happen out of order
		{
			VkRenderingAttachmentInfo color_attachment = rhi_helpers::attachment_info(
				draw_image.image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			VkRenderingAttachmentInfo depth_attachment = rhi_helpers::depth_attachment_info(
				depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
			VkRenderingInfo render_info = rhi_helpers::rendering_info(swapchain_extent, color_attachment, depth_attachment);
			// begin render pass
			vkCmdBeginRendering(cmd, &render_info);
		}
	}

	return VK_SUCCESS;
}


VkResult rhi::end_frame()
{
	auto [opt_command_buffers, opt_image_available_semaphores, opt_render_finished_semaphores, opt_in_flight_fence,
		opt_command_pool, opt_frame_descriptors, opt_gpu_scene_buffer] = frame_datas_[current_frame_];
	{
		if (!opt_command_buffers.has_value()) return VK_NOT_READY;
		if (!opt_image_available_semaphores.has_value()) return VK_NOT_READY;
		if (!opt_render_finished_semaphores.has_value()) return VK_NOT_READY;
		if (!opt_in_flight_fence.has_value()) return VK_NOT_READY;
		if (!opt_command_pool.has_value()) return VK_NOT_READY;
	}

	VkCommandBuffer cmd = opt_command_buffers.value();
	VkSemaphore image_available = opt_image_available_semaphores.value();
	VkSemaphore rendered_finished = opt_render_finished_semaphores.value();
	VkFence fence = opt_in_flight_fence.value();
	descriptor_allocator_growable frame_descriptors = opt_frame_descriptors.value();
	uint32_t image_index = current_swapchain_image_index_;
	VkImage image = swap_chain_images_[image_index];
	VkImageView image_view = swap_chain_image_views_[image_index];
	current_swapchain_image_index_ = image_index;

	allocated_image draw_image = draw_image_.value();
	{
		VkResult result;
		// end render pass
		vkCmdEndRendering(cmd);
		{
			// blit the draw image to the swapchain image
			transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			transition_image(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			rhi_helpers::blit_images(cmd, draw_image.image, image, draw_extent_, swapchain_extent);
		}
		{
			// draw ui onto swapchain image
			transition_image(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			result = render_ui(cmd, image_view);
			if (result != VK_SUCCESS) return result;
		}
		{
			// Transition swapchain image for presentation
			transition_image(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			result = vkEndCommandBuffer(cmd);
			if (result != VK_SUCCESS) return result;
		}

		{
			// submit recorded commands to the queue
			VkCommandBufferSubmitInfo cmd_buffer_submit_info{};
			cmd_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
			cmd_buffer_submit_info.pNext = nullptr;
			cmd_buffer_submit_info.commandBuffer = cmd;
			cmd_buffer_submit_info.deviceMask = 0;

			VkSemaphoreSubmitInfo wait_info{};
			wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			wait_info.pNext = nullptr;
			wait_info.semaphore = image_available;
			wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
			wait_info.deviceIndex = 0;
			wait_info.value = 1;

			VkSemaphoreSubmitInfo signal_info{};
			signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			signal_info.pNext = nullptr;
			signal_info.semaphore = rendered_finished;
			signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
			signal_info.deviceIndex = 0;
			signal_info.value = 1;

			VkSubmitInfo2 submit_info{};
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
			submit_info.pNext = nullptr;
			// This should be obvious, but it wasn't for me. Wait blocks this submit until the semaphore assigned to it is signaled.
			// To state again, wait semaphores means the submit waits for the semaphore in wait info to be signaled before it begins
			// in this case we're waiting for the image we requested at the beginning of render frame to be available.
			// What we have to remember is that up until this point we've only *recorded* the commands we want to execute
			// on to the command data. This submit will actually start the process of telling the GPU to do the commands.
			// None of that has happened yet. We're declaring future work there. Up until this point the only actual work we've done
			// is request an image and record commands we want to perform unto the command data.
			// vkAcquireNextImageKHR will signal `imageAvailable` when done, `imageAvailable` is the semaphore in wait_info
			submit_info.waitSemaphoreInfoCount = 1;
			submit_info.pWaitSemaphoreInfos = &wait_info;
			submit_info.signalSemaphoreInfoCount = 1;
			// When submit is done it signals this `signalInfo` semaphore and unblocks anything which is waiting for it.
			submit_info.pSignalSemaphoreInfos = &signal_info;
			submit_info.commandBufferInfoCount = 1;
			submit_info.pCommandBufferInfos = &cmd_buffer_submit_info;
			result = vkQueueSubmit2(present_queue_.value(), 1, &submit_info, fence);
			if (result != VK_SUCCESS) return result;
		}
		{
			// Queue image for presentation
			VkSwapchainKHR swap_chains[] = { swapchain_.value() };
			VkPresentInfoKHR present_info{};
			present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			present_info.waitSemaphoreCount = 1;
			// Present is waiting for the submit queue above to signal `rendered_finished`
			present_info.pWaitSemaphores = &rendered_finished;
			present_info.swapchainCount = 1;
			present_info.pSwapchains = swap_chains;
			present_info.pImageIndices = &image_index;

			result = vkQueuePresentKHR(present_queue_.value(), &present_info);
			if (result != VK_SUCCESS) return result;
		}
	}

	current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
	return VK_SUCCESS;
}

VkResult rhi::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& record_func) const
{
	const VkDevice device = opt_device.value();
	VkResult result = vkResetFences(device, 1, &imm_fence_.value());
	if (result != VK_SUCCESS) return result;

	result = vkResetCommandBuffer(imm_command_buffer_.value(), 0);
	if (result != VK_SUCCESS) return result;

	const VkCommandBuffer cmd = imm_command_buffer_.value();

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	result = vkBeginCommandBuffer(cmd, &begin_info);
	if (result != VK_SUCCESS) return result;

	record_func(cmd);

	result = vkEndCommandBuffer(cmd);
	if (result != VK_SUCCESS) return result;

	VkCommandBufferSubmitInfo cmd_buffer_submit_info{};
	cmd_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd_buffer_submit_info.pNext = nullptr;
	cmd_buffer_submit_info.commandBuffer = cmd;
	cmd_buffer_submit_info.deviceMask = 0;
	VkSubmitInfo2 submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.pNext = nullptr;

	submit_info.waitSemaphoreInfoCount = 0;
	submit_info.pWaitSemaphoreInfos = nullptr;
	submit_info.signalSemaphoreInfoCount = 0;
	submit_info.pSignalSemaphoreInfos = nullptr;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &cmd_buffer_submit_info;

	result = vkQueueSubmit2(present_queue_.value(), 1, &submit_info, imm_fence_.value());
	if (result != VK_SUCCESS) return result;

	result = vkWaitForFences(device, 1, &imm_fence_.value(), true, 9999999999);
	if (result != VK_SUCCESS) return result;

	return VK_SUCCESS;
}