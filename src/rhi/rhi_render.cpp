#include "RHI.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void Rhi::transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask) {
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = aspectMask;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageMemoryBarrier2 imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.pNext = nullptr;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.subresourceRange = subresourceRange;
	imageBarrier.image = image;

	VkDependencyInfo dependencyInfo{};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext = nullptr;

	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

VkResult Rhi::renderFrame() {
	VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
	VkSemaphore imageAvailable = m_imageAvailableSemaphores[m_currentFrame];
	VkSemaphore renderedFinisished = m_renderFinishedSemaphores[m_currentFrame];
	VkFence fence = m_inFlightFence[m_currentFrame];
	VkResult result;
	VkDevice device = m_device.value();

	result = vkWaitForFences(device, 1, &fence, true, 1000000000);
	if (result != VK_SUCCESS) return result;

	uint32_t imageIndex;
	// vkAcquireNextImageKHR will signal the imageAvailable semaphore which the submit queue call will wait for below.
	vkAcquireNextImageKHR(m_device.value(), m_swapchain.value(), UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);
	VkImage image = m_swapChainImages[imageIndex];
	VkImageView imageView = m_swapChainImageViews[imageIndex];

	AllocatedImage drawImage = m_drawImage.value();
	AllocatedImage depthImage = m_depthImage.value();
	m_drawExtent.width = drawImage.imageExtent.width;
	m_drawExtent.height = drawImage.imageExtent.height;

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
		VkExtent2D swapChainExtent = m_swapChainExtent;
		{
			vkCmdSetRasterizerDiscardEnableEXT(cmd, VK_FALSE);
			VkColorBlendEquationEXT colorBlendEquationEXT{};
			vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &colorBlendEquationEXT);
		}
		{
			vkCmdSetPrimitiveTopologyEXT(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			vkCmdSetPrimitiveRestartEnableEXT(cmd, VK_FALSE);
			vkCmdSetRasterizationSamplesEXT(cmd, VK_SAMPLE_COUNT_1_BIT);
		}
		{
			const VkSampleMask sample_mask = 0x1;
			vkCmdSetSampleMaskEXT(cmd, VK_SAMPLE_COUNT_1_BIT, &sample_mask);
		}
		{
			vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);
			vkCmdSetCullModeEXT(cmd, VK_TRUE);
			vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);
		}
		{
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)swapChainExtent.width;
			viewport.height = (float)swapChainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetViewportWithCountEXT(cmd, 1, &viewport);
		}
		{
			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = swapChainExtent;
			vkCmdSetScissor(cmd, 0, 1, &scissor);
			vkCmdSetScissorWithCountEXT(cmd, 1, &scissor);
		}
		{
			vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
			vkCmdSetDepthTestEnableEXT(cmd, VK_TRUE);
			vkCmdSetDepthWriteEnableEXT(cmd, VK_TRUE);
			vkCmdSetDepthCompareOpEXT(cmd, VK_COMPARE_OP_GREATER_OR_EQUAL);
			vkCmdSetDepthBoundsTestEnableEXT(cmd, VK_FALSE);
			vkCmdSetDepthBiasEnableEXT(cmd, VK_FALSE);
			vkCmdSetStencilTestEnableEXT(cmd, VK_FALSE);
			vkCmdSetLogicOpEnableEXT(cmd, VK_FALSE);
			vkCmdSetDepthBounds(cmd, 0.0f, 1.0f);
		}
		{
			VkBool32 color_blend_enables[] = { VK_FALSE };
			vkCmdSetColorBlendEnableEXT(cmd, 0, 1, color_blend_enables);
		}
		{
			VkColorComponentFlags color_component_flags[] = { VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT };
			vkCmdSetColorWriteMaskEXT(cmd, 0, 1, color_component_flags);
		}
		{
			vkCmdSetVertexInputEXT(cmd, 0, nullptr, 0, nullptr);
		}
	}

	{
		// Clear image. This transition means that all the commands recorded before now happen before
		// any calls after. The calls themselves before this may have executed in any order up until this point.
		transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
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
		transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		transitionImage(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
		//  and the subsequent happening between vkCmdBeginRendering and vkCmdEndRendering happen after this, but may happen out of order
		{
			VkRenderingAttachmentInfo colorAttachment = attachmentInfo(drawImage.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			VkRenderingAttachmentInfo depthAttachment = depthAttachmentInfo(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
			VkRenderingInfo renderInfo = renderingInfo(m_swapChainExtent, colorAttachment, depthAttachment);
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
			vkCmdBindShadersEXT(cmd, 2, stages, m_shaders.data());
			const VkShaderStageFlagBits unusedStages[3] =
			{
				VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
				VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
				VK_SHADER_STAGE_GEOMETRY_BIT
			};
			vkCmdBindShadersEXT(cmd, 3, unusedStages, NULL);

			GPUDrawPushConstants push_constants;
			glm::mat4 m = glm::mat4(1.0f);
			m = glm::rotate(m, m_model_rot_x, glm::vec3(1, 0, 0));
			m = glm::rotate(m, m_model_rot_y, glm::vec3(0, 1, 0));
			m = glm::rotate(m, m_model_rot_z, glm::vec3(0, 0, 1));
			m = glm::scale(m, glm::vec3(m_model_scale, m_model_scale, m_model_scale));
			glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3{ m_model_x, m_model_y, m_model_z });
			glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)m_drawExtent.width / (float)m_drawExtent.height, 1000.f, 0.1f);
			push_constants.worldMatrix = projection * view * m;

			if (m_testMeshes.size() > 0) {
				push_constants.vertexBuffer = m_testMeshes[2]->meshBuffers.vertexBufferAddress;
				vkCmdPushConstants(cmd, m_shaderPL.value(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &push_constants);
				vkCmdBindIndexBuffer(cmd, m_testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, m_testMeshes[2]->surfaces[0].count, 1, m_testMeshes[2]->surfaces[0].startIndex, 0, 0);
			}

			vkCmdEndDebugUtilsLabelEXT(cmd);
		}
	}

	{
		// end app rendering
		vkCmdEndRendering(cmd);
		{
			// blit the draw image to the swapchain image
			transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			transitionImage(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			blitImages(cmd, drawImage.image, image, m_drawExtent, m_swapChainExtent);
		}
		{
			// draw ui onto swapchain image
			transitionImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			result = renderUI(cmd, imageView);
			if (result != VK_SUCCESS) return result;
		}
		{
			// Transition swapchain image for presentation
			transitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
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
			waitInfo.semaphore = imageAvailable;
			waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
			waitInfo.deviceIndex = 0;
			waitInfo.value = 1;

			VkSemaphoreSubmitInfo signalInfo = {};
			signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			signalInfo.pNext = nullptr;
			signalInfo.semaphore = renderedFinisished;
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
			result = vkQueueSubmit2(m_presentQueue.value(), 1, &submitInfo, fence);
			if (result != VK_SUCCESS) return result;
		}
		{
			// Queue image for presentation
			VkSwapchainKHR swapChains[] = { m_swapchain.value() };
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			// Present is waiting for the submit queue above to signal `renderedFinisished`
			presentInfo.pWaitSemaphores = &renderedFinisished;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;

			result = vkQueuePresentKHR(m_presentQueue.value(), &presentInfo);
			if (result != VK_SUCCESS) return result;
		}
	}

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return VK_SUCCESS;
}

VkResult Rhi::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& recordFunc) {
	VkResult result;

	VkDevice device = m_device.value();
	result = vkResetFences(device, 1, &m_immFence.value());
	if (result != VK_SUCCESS) return result;

	result = vkResetCommandBuffer(m_immCommandBuffer.value(), 0);
	if (result != VK_SUCCESS) return result;

	VkCommandBuffer cmd = m_immCommandBuffer.value();

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

	result = vkQueueSubmit2(m_presentQueue.value(), 1, &submitInfo, m_immFence.value());
	if (result != VK_SUCCESS) return result;

	result = vkWaitForFences(device, 1, &m_immFence.value(), true, 9999999999);
	if (result != VK_SUCCESS) return result;

	return VK_SUCCESS;
}