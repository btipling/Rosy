#include "RHI.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void rhi::transition_image(const VkCommandBuffer cmd, const VkImage image, const VkImageLayout current_layout, const VkImageLayout new_layout, const VkImageAspectFlags aspect_mask) {
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = aspect_mask;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = VK_REMAINING_MIP_LEVELS;
	subresource_range.baseArrayLayer = 0;
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

VkResult rhi::render_frame() {
	auto [command_buffers, image_available_semaphores, render_finished_semaphores, in_flight_fence, opt_command_pool] = frame_datas_[current_frame_];
	if (!opt_command_pool.has_value()) return VK_NOT_READY;
	VkCommandPool command_pool = opt_command_pool.value();
	VkCommandBuffer cmd = command_buffers;
	VkSemaphore image_available = image_available_semaphores;
	VkSemaphore rendered_finished = render_finished_semaphores;
	VkFence fence = in_flight_fence;
	VkResult result;
	VkDevice device = device_.value();

	result = vkWaitForFences(device, 1, &fence, true, 1000000000);
	if (result != VK_SUCCESS) return result;

	uint32_t imageIndex;
	// vkAcquireNextImageKHR will signal the imageAvailable semaphore which the submit queue call will wait for below.
	vkAcquireNextImageKHR(device_.value(), swapchain_.value(), UINT64_MAX, image_available, VK_NULL_HANDLE, &imageIndex);
	VkImage image = swap_chain_images_[imageIndex];
	VkImageView imageView = swap_chain_image_views_[imageIndex];

	allocated_image drawImage = draw_image_.value();
	allocated_image depthImage = depth_image_.value();
	draw_extent_.width = std::min(swapchain_extent_.width, drawImage.image_extent.width) * render_scale_;
	draw_extent_.height = std::min(swapchain_extent_.height, drawImage.image_extent.height) * render_scale_;

	vkResetFences(device, 1, &fence);
	{
		// Start recording commands. This records commands that aren't actually submitted to a queue to do anything with until 
		// 1. The submit queue call has been made but also
		// 2. The wait semaphore given to the submit queue has been signeled, which
		// 3. Doesn't happen until the image we requested with vkAcquireNextImageKHR has been made available
		result = vkResetCommandBuffer(cmd, 0);
		if (result != VK_SUCCESS) return result;

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		result = vkBeginCommandBuffer(cmd, &beginInfo);
		if (result != VK_SUCCESS) return result;
	}


	{
		// Configure the dynamic shader pipeline
		set_rendering_defaults(cmd);
		toggle_culling(cmd, VK_TRUE);
		toggle_wire_frame(cmd, toggle_wire_frame_);
		set_view_port(cmd, swapchain_extent_);
		toggle_depth(cmd, VK_TRUE);
		switch (blend_mode_) {
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
		transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
		// vkCmdClearColorImage is guaranteed to happen after previous calls.
		VkClearColorValue clearValue;
		clearValue = { { 0.0f, 0.05f, 0.1f, 1.0f } };
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		vkCmdClearColorImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &subresourceRange);
	}
	{
		// Start dynamic render pass, again this sets a barrier between vkCmdClearColorImage and what happens after
		transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		transition_image(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
		//  and the subsequent happening between vkCmdBeginRendering and vkCmdEndRendering happen after this, but may happen out of order
		{
			VkRenderingAttachmentInfo colorAttachment = attachment_info(drawImage.image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			VkRenderingAttachmentInfo depthAttachment = depth_attachment_info(depthImage.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
			VkRenderingInfo renderInfo = rendering_info(swapchain_extent_, colorAttachment, depthAttachment);
			vkCmdBeginRendering(cmd, &renderInfo);
		}

	}
	{
		// triangle

		{
			const VkDebugUtilsLabelEXT triangleLabel =
			{
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
				.pNext = NULL,
				.pLabelName = "triangleLabel",
				.color = { 1.0f, 0.0f, 0.0f, 1.0f },
			};
			vkCmdBeginDebugUtilsLabelEXT(cmd, &triangleLabel);
			const VkShaderStageFlagBits stages[2] =
			{
				VK_SHADER_STAGE_VERTEX_BIT,
				VK_SHADER_STAGE_FRAGMENT_BIT
			};
			vkCmdBindShadersEXT(cmd, 2, stages, shaders_.data());
			const VkShaderStageFlagBits unusedStages[3] =
			{
				VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
				VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
				VK_SHADER_STAGE_GEOMETRY_BIT
			};
			vkCmdBindShadersEXT(cmd, 3, unusedStages, NULL);

			gpu_draw_push_constants push_constants;
			glm::mat4 m = glm::mat4(1.0f);

			glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, -10.0f); 
			glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
			glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

			glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);

			m = glm::translate(m, glm::vec3{ model_x_, model_y_, model_z_ });
			m = glm::rotate(m, model_rot_x_, glm::vec3(1, 0, 0));
			m = glm::rotate(m, model_rot_y_, glm::vec3(0, 1, 0));
			m = glm::rotate(m, model_rot_z_, glm::vec3(0, 0, 1));
			m = glm::scale(m, glm::vec3(model_scale_, model_scale_, model_scale_));

			float znear = 0.1f;
			float zfar = 1000.0f;
			float aspect = (float)draw_extent_.width / (float)draw_extent_.height;
			constexpr float fov = glm::radians(70.0f);
			float h = 1.0 / tan(fov * 0.5);
			float w = h / aspect;
			float a = -znear / (zfar - znear);
			float b = (znear * zfar) / (zfar - znear);

			glm::mat4 proj(0.0f);

			proj[0][0] = w;
			proj[1][1] = -h;

			proj[2][2] = a;
			proj[3][2] = b;
			proj[2][3] = 1.0f;
			push_constants.world_matrix = proj * view * m;

			if (test_meshes_.size() > 0) {
				push_constants.vertex_buffer = test_meshes_[2]->mesh_buffers.vertex_buffer_address;
				vkCmdPushConstants(cmd, shader_pl_.value(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &push_constants);
				vkCmdBindIndexBuffer(cmd, test_meshes_[2]->mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, test_meshes_[2]->surfaces[0].count, 1, test_meshes_[2]->surfaces[0].start_index, 0, 0);
			}

			vkCmdEndDebugUtilsLabelEXT(cmd);
		}
	}

	{
		// end app rendering
		vkCmdEndRendering(cmd);
		{
			// blit the draw image to the swapchain image
			transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			transition_image(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			blit_images(cmd, drawImage.image, image, draw_extent_, swapchain_extent_);
		}
		{
			// draw ui onto swapchain image
			transition_image(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			result = render_ui(cmd, imageView);
			if (result != VK_SUCCESS) return result;
		}
		{
			// Transition swapchain image for presentation
			transition_image(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
			result = vkEndCommandBuffer(cmd);
			if (result != VK_SUCCESS) return result;
		}

		{
			// submit recorded commands to the queue
			VkCommandBufferSubmitInfo cmdBufferSubmitInfo = {};
			cmdBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
			cmdBufferSubmitInfo.pNext = nullptr;
			cmdBufferSubmitInfo.commandBuffer = cmd;
			cmdBufferSubmitInfo.deviceMask = 0;

			VkSemaphoreSubmitInfo waitInfo = {};
			waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			waitInfo.pNext = nullptr;
			waitInfo.semaphore = image_available;
			waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
			waitInfo.deviceIndex = 0;
			waitInfo.value = 1;

			VkSemaphoreSubmitInfo signalInfo = {};
			signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			signalInfo.pNext = nullptr;
			signalInfo.semaphore = rendered_finished;
			signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
			signalInfo.deviceIndex = 0;
			signalInfo.value = 1;

			VkSubmitInfo2 submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
			submitInfo.pNext = nullptr;
			// This should be obvious but it wasn't for me. Wait blocks this submit until the semaphore assigned to it is signaled.
			// To state again, wait semaphores means the submit waits for the semaphore in wait info to be signaled before it begins
			// in this case we're waiting for the image we requested at the beginning of render frame to be available.
			// What we have to remember is that up until this point we've only *recorded* the commands we want to execute
			// on to the command buffer. This submit will actually start the process of telling the GPU to do the commands.
			// None of that has happened yet. We're declaring future work there. Up until this point the only actual work we've done
			// is request an image and record commands we want to perform unto the command buffer.
			// vkAcquireNextImageKHR will signal `imageAvailable` when done, `imageAvailable` is the semaphore in waitinfo
			submitInfo.waitSemaphoreInfoCount = 1;
			submitInfo.pWaitSemaphoreInfos = &waitInfo;
			submitInfo.signalSemaphoreInfoCount = 1;
			// When submit is done it signals this `signalInfo` semaphore and unbocks anything which is waiting for it.
			submitInfo.pSignalSemaphoreInfos = &signalInfo;
			submitInfo.commandBufferInfoCount = 1;
			submitInfo.pCommandBufferInfos = &cmdBufferSubmitInfo;
			result = vkQueueSubmit2(present_queue_.value(), 1, &submitInfo, fence);
			if (result != VK_SUCCESS) return result;
		}
		{
			// Queue image for presentation
			VkSwapchainKHR swapChains[] = { swapchain_.value() };
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			// Present is waiting for the submit queue above to signal `renderedFinisished`
			presentInfo.pWaitSemaphores = &rendered_finished;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;

			result = vkQueuePresentKHR(present_queue_.value(), &presentInfo);
			if (result != VK_SUCCESS) return result;
		}
	}

	current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
	return VK_SUCCESS;
}

VkResult rhi::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& recordFunc) {
	VkResult result;

	VkDevice device = device_.value();
	result = vkResetFences(device, 1, &imm_fence_.value());
	if (result != VK_SUCCESS) return result;

	result = vkResetCommandBuffer(imm_command_buffer_.value(), 0);
	if (result != VK_SUCCESS) return result;

	VkCommandBuffer cmd = imm_command_buffer_.value();

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	result = vkBeginCommandBuffer(cmd, &beginInfo);
	if (result != VK_SUCCESS) return result;

	recordFunc(cmd);

	result = vkEndCommandBuffer(cmd);
	if (result != VK_SUCCESS) return result;

	VkCommandBufferSubmitInfo cmdBufferSubmitInfo = {};
	cmdBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdBufferSubmitInfo.pNext = nullptr;
	cmdBufferSubmitInfo.commandBuffer = cmd;
	cmdBufferSubmitInfo.deviceMask = 0;
	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = nullptr;

	submitInfo.waitSemaphoreInfoCount = 0;
	submitInfo.pWaitSemaphoreInfos = nullptr;
	submitInfo.signalSemaphoreInfoCount = 0;
	submitInfo.pSignalSemaphoreInfos = nullptr;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdBufferSubmitInfo;

	result = vkQueueSubmit2(present_queue_.value(), 1, &submitInfo, imm_fence_.value());
	if (result != VK_SUCCESS) return result;

	result = vkWaitForFences(device, 1, &imm_fence_.value(), true, 9999999999);
	if (result != VK_SUCCESS) return result;

	return VK_SUCCESS;
}