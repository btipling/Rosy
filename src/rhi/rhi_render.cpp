#include "RHI.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void rhi::transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageLayout current_layout,
                           const VkImageLayout new_layout)
{
	VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
		                                 ? VK_IMAGE_ASPECT_DEPTH_BIT
		                                 : VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageSubresourceRange subresource_range = create_img_subresource_range(aspect_mask);
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

VkResult rhi::render_frame()
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

	VkCommandPool command_pool = opt_command_pool.value();
	VkCommandBuffer cmd = opt_command_buffers.value();
	VkSemaphore image_available = opt_image_available_semaphores.value();
	VkSemaphore rendered_finished = opt_render_finished_semaphores.value();
	VkFence fence = opt_in_flight_fence.value();
	descriptor_allocator_growable frame_descriptors = opt_frame_descriptors.value();

	VkResult result;
	VkDevice device = device_.value();

	result = vkWaitForFences(device, 1, &fence, true, 1000000000);
	if (result != VK_SUCCESS) return result;
	frame_descriptors.clear_pools(device);
	if (opt_gpu_scene_buffer.has_value()) destroy_buffer(opt_gpu_scene_buffer.value());
	frame_datas_[current_frame_].gpu_scene_buffer = std::nullopt;

	uint32_t image_index;
	// vkAcquireNextImageKHR will signal the imageAvailable semaphore which the submit queue call will wait for below.
	vkAcquireNextImageKHR(device_.value(), swapchain_.value(), UINT64_MAX, image_available, VK_NULL_HANDLE,
	                      &image_index);
	VkImage image = swap_chain_images_[image_index];
	VkImageView image_view = swap_chain_image_views_[image_index];

	allocated_image draw_image = draw_image_.value();
	allocated_image depth_image = depth_image_.value();
	draw_extent_.width = std::min(swapchain_extent_.width, draw_image.image_extent.width) * render_scale_;
	draw_extent_.height = std::min(swapchain_extent_.height, draw_image.image_extent.height) * render_scale_;

	vkResetFences(device, 1, &fence);
	{
		// Start recording commands. This records commands that aren't actually submitted to a queue to do anything with until 
		// 1. The submit queue call has been made but also
		// 2. The wait semaphore given to the submit queue has been signaled, which
		// 3. Doesn't happen until the image we requested with vkAcquireNextImageKHR has been made available
		result = vkResetCommandBuffer(cmd, 0);
		if (result != VK_SUCCESS) return result;

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		result = vkBeginCommandBuffer(cmd, &begin_info);
		if (result != VK_SUCCESS) return result;
	}


	{
		// Configure the dynamic shader pipeline
		set_rendering_defaults(cmd);
		toggle_culling(cmd, VK_TRUE);
		toggle_wire_frame(cmd, toggle_wire_frame_);
		set_view_port(cmd, swapchain_extent_);
		toggle_depth(cmd, VK_TRUE);
		// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
		switch (blend_mode_)
		{
		case 0:
			disable_blending(cmd);
			break;
		case 1:
			enable_blending_additive(cmd);
			break;
		case 2:
			enable_blending_alpha_blend(cmd);
			break;
		}
	}

	{
		// Clear image. This transition means that all the commands recorded before now happen before
		// any calls after. The calls themselves before this may have executed in any order up until this point.
		transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		// vkCmdClearColorImage is guaranteed to happen after previous calls.
		VkClearColorValue clear_value;
		clear_value = {{0.0f, 0.05f, 0.1f, 1.0f}};
		VkImageSubresourceRange subresource_range = create_img_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &subresource_range);
	}
	{
		// Start dynamic render pass, again this sets a barrier between vkCmdClearColorImage and what happens after
		transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		transition_image(cmd, depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		//  and the subsequent happening between vkCmdBeginRendering and vkCmdEndRendering happen after this, but may happen out of order
		{
			VkRenderingAttachmentInfo color_attachment = attachment_info(
				draw_image.image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			VkRenderingAttachmentInfo depth_attachment = depth_attachment_info(
				depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
			VkRenderingInfo render_info = rendering_info(swapchain_extent_, color_attachment, depth_attachment);
			vkCmdBeginRendering(cmd, &render_info);
		}
	}
	{
		// meshes
		{
			//allocate a new uniform buffer for the scene data
			auto [result, buffer] = create_buffer(sizeof(gpu_scene_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			                                      VMA_MEMORY_USAGE_AUTO);
			if (result != VK_SUCCESS) return result;
			allocated_buffer gpu_scene_buffer = buffer;
			frame_datas_[current_frame_].gpu_scene_buffer = gpu_scene_buffer;

			// bind a texture
			auto [image_set_result, image_set] = frame_descriptors.allocate(device, single_image_descriptor_layout_.value());
			if (image_set_result != VK_SUCCESS) return result;
			{
				descriptor_writer writer;
				writer.write_image(0, error_checkerboard_image_->image_view, default_sampler_nearest_.value(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

				writer.update_set(device, image_set);
			}
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shader_pl_.value(), 0, 1, &image_set, 0, nullptr);

			//create a descriptor set that binds that buffer and update it
			auto descriptor_result = frame_descriptors.allocate(device, gpu_scene_data_descriptor_layout_.value());
			if (descriptor_result.result != VK_SUCCESS) return descriptor_result.result;
			VkDescriptorSet global_descriptor = descriptor_result.set;

			descriptor_writer writer;
			writer.write_buffer(0, gpu_scene_buffer.buffer, sizeof(gpu_scene_data), 0,
			                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			writer.update_set(device, global_descriptor);

			constexpr VkDebugUtilsLabelEXT mesh_draw_label =
			{
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
				.pNext = nullptr,
				.pLabelName = "meshes",
				.color = {1.0f, 0.0f, 0.0f, 1.0f},
			};
			vkCmdBeginDebugUtilsLabelEXT(cmd, &mesh_draw_label);
			constexpr VkShaderStageFlagBits stages[2] =
			{
				VK_SHADER_STAGE_VERTEX_BIT,
				VK_SHADER_STAGE_FRAGMENT_BIT
			};
			vkCmdBindShadersEXT(cmd, 2, stages, shaders_.data());
			constexpr VkShaderStageFlagBits unused_stages[3] =
			{
				VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
				VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
				VK_SHADER_STAGE_GEOMETRY_BIT
			};
			vkCmdBindShadersEXT(cmd, 3, unused_stages, nullptr);

			gpu_draw_push_constants push_constants;
			auto m = glm::mat4(1.0f);

			auto camera_pos = glm::vec3(0.0f, 0.0f, -10.0f);
			auto camera_target = glm::vec3(0.0f, 0.0f, 0.0f);
			auto camera_up = glm::vec3(0.0f, 1.0f, 0.0f);

			glm::mat4 view = lookAt(camera_pos, camera_target, camera_up);

			m = translate(m, glm::vec3{model_x_, model_y_, model_z_});
			m = rotate(m, model_rot_x_, glm::vec3(1, 0, 0));
			m = rotate(m, model_rot_y_, glm::vec3(0, 1, 0));
			m = rotate(m, model_rot_z_, glm::vec3(0, 0, 1));
			m = scale(m, glm::vec3(model_scale_, model_scale_, model_scale_));

			float z_near = 0.1f;
			float z_far = 1000.0f;
			float aspect = static_cast<float>(draw_extent_.width) / static_cast<float>(draw_extent_.height);
			constexpr float fov = glm::radians(70.0f);
			float h = 1.0 / tan(fov * 0.5);
			float w = h / aspect;
			float a = -z_near / (z_far - z_near);
			float b = (z_near * z_far) / (z_far - z_near);

			glm::mat4 proj(0.0f);

			proj[0][0] = w;
			proj[1][1] = -h;

			proj[2][2] = a;
			proj[3][2] = b;
			proj[2][3] = 1.0f;
			push_constants.world_matrix = proj * view * m;

			if (test_meshes_.size() > 0)
			{
				push_constants.vertex_buffer = test_meshes_[2]->mesh_buffers.vertex_buffer_address;
				vkCmdPushConstants(cmd, shader_pl_.value(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants),
				                   &push_constants);
				vkCmdBindIndexBuffer(cmd, test_meshes_[2]->mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, test_meshes_[2]->surfaces[0].count, 1, test_meshes_[2]->surfaces[0].start_index,
				                 0, 0);
			}

			vkCmdEndDebugUtilsLabelEXT(cmd);
		}
	}

	{
		// end app rendering
		vkCmdEndRendering(cmd);
		{
			// blit the draw image to the swapchain image
			transition_image(cmd, draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			transition_image(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			blit_images(cmd, draw_image.image, image, draw_extent_, swapchain_extent_);
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
			VkCommandBufferSubmitInfo cmd_buffer_submit_info = {};
			cmd_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
			cmd_buffer_submit_info.pNext = nullptr;
			cmd_buffer_submit_info.commandBuffer = cmd;
			cmd_buffer_submit_info.deviceMask = 0;

			VkSemaphoreSubmitInfo wait_info = {};
			wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			wait_info.pNext = nullptr;
			wait_info.semaphore = image_available;
			wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
			wait_info.deviceIndex = 0;
			wait_info.value = 1;

			VkSemaphoreSubmitInfo signal_info = {};
			signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			signal_info.pNext = nullptr;
			signal_info.semaphore = rendered_finished;
			signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
			signal_info.deviceIndex = 0;
			signal_info.value = 1;

			VkSubmitInfo2 submit_info = {};
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
			submit_info.pNext = nullptr;
			// This should be obvious, but it wasn't for me. Wait blocks this submit until the semaphore assigned to it is signaled.
			// To state again, wait semaphores means the submit waits for the semaphore in wait info to be signaled before it begins
			// in this case we're waiting for the image we requested at the beginning of render frame to be available.
			// What we have to remember is that up until this point we've only *recorded* the commands we want to execute
			// on to the command buffer. This submit will actually start the process of telling the GPU to do the commands.
			// None of that has happened yet. We're declaring future work there. Up until this point the only actual work we've done
			// is request an image and record commands we want to perform unto the command buffer.
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
			VkSwapchainKHR swap_chains[] = {swapchain_.value()};
			VkPresentInfoKHR present_info = {};
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
	const VkDevice device = device_.value();
	VkResult result = vkResetFences(device, 1, &imm_fence_.value());
	if (result != VK_SUCCESS) return result;

	result = vkResetCommandBuffer(imm_command_buffer_.value(), 0);
	if (result != VK_SUCCESS) return result;

	const VkCommandBuffer cmd = imm_command_buffer_.value();

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	result = vkBeginCommandBuffer(cmd, &begin_info);
	if (result != VK_SUCCESS) return result;

	record_func(cmd);

	result = vkEndCommandBuffer(cmd);
	if (result != VK_SUCCESS) return result;

	VkCommandBufferSubmitInfo cmd_buffer_submit_info = {};
	cmd_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd_buffer_submit_info.pNext = nullptr;
	cmd_buffer_submit_info.commandBuffer = cmd;
	cmd_buffer_submit_info.deviceMask = 0;
	VkSubmitInfo2 submit_info = {};
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
