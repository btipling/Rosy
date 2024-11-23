#include "RHI.h"

void Rhi::debug() {
	rosy_utils::DebugPrintA("RHI Debug Data::");
	if (!m_instance.has_value()) {
		rosy_utils::DebugPrintA("No instance!");
		return;
	}

	if (!m_physicalDeviceProperties.has_value()) {
		rosy_utils::DebugPrintA("No physical device!");
		return;
	}
	VkPhysicalDeviceProperties deviceProperties = m_physicalDeviceProperties.value();
	VkPhysicalDeviceFeatures deviceFeatures = m_supportedFeatures.value();
	VkPhysicalDeviceMemoryProperties deviceMemProps = m_physicalDeviceMemoryProperties.value();
	std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData = m_queueFamilyProperties.value();
	rosy_utils::DebugPrintA("result device property vendor %s \n", deviceProperties.deviceName);
	rosy_utils::DebugPrintA("result: vendor: %u \n", deviceProperties.vendorID);

	rosy_utils::DebugPrintA("has multiDrawIndirect? %d \n", deviceFeatures.multiDrawIndirect);
	for (uint32_t i = 0; i < deviceMemProps.memoryHeapCount; i++) {
		rosy_utils::DebugPrintA("memory size: %d\n", deviceMemProps.memoryHeaps[i].size);
		rosy_utils::DebugPrintA("memory flags: %d\n", deviceMemProps.memoryHeaps[i].flags);
	}
	for (const VkQueueFamilyProperties& qfmp : queueFamilyPropertiesData) {
		rosy_utils::DebugPrintA("queue count: %d and time bits: %d\n", qfmp.queueCount, qfmp.timestampValidBits);
		if (qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT)) {
			rosy_utils::DebugPrintA("VkQueueFamilyProperties got all the things\n");
		}
		else {
			rosy_utils::DebugPrintA("VkQueueFamilyProperties missing stuff\n");
		}
	}
	rosy_utils::DebugPrintA("Selected queue index %d with count: %d\n", m_queueIndex, m_queueCount);

}

void Rhi::transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout) {
	VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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
			vkCmdSetDepthTestEnableEXT(cmd, VK_FALSE);
			vkCmdSetDepthWriteEnableEXT(cmd, VK_FALSE);
			vkCmdSetDepthCompareOpEXT(cmd, VK_COMPARE_OP_GREATER);
			vkCmdSetDepthBoundsTestEnableEXT(cmd, VK_FALSE);
			vkCmdSetDepthBiasEnableEXT(cmd, VK_FALSE);
			vkCmdSetStencilTestEnableEXT(cmd, VK_FALSE);
			vkCmdSetLogicOpEnableEXT(cmd, VK_FALSE);
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
		{
			glm::mat4 m = glm::mat4(1.0f);
			m = glm::rotate(m, m_triangle_rot, glm::vec3(0, 0, 1));
			//rosy_utils::DebugPrintA("Matrix:\n"
			//	"[%.2f %.2f %.2f %.2f]\n"
			//	"[%.2f %.2f %.2f %.2f]\n"
			//	"[%.2f %.2f %.2f %.2f]\n"
			//	"[%.2f %.2f %.2f %.2f]\n",
			//	m[0][0], m[0][1], m[0][2], m[0][3],
			//	m[1][0], m[1][1], m[1][2], m[1][3],
			//	m[2][0], m[2][1], m[2][2], m[2][3],
			//	m[3][0], m[3][1], m[3][2], m[3][3]);
			//rosy_utils::DebugPrintA("glm::mat4 size: %d\n", sizeof(glm::mat4));
			vkCmdPushConstants(cmd, m_shaderPL.value(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &m);
		}
	}

	{
		// Clear image. This transition means that all the commands recorded before now happen before
		// any calls after. The calls themselves before this may have executed in any order up until this point.
		transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
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
		transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		//  and the subsequent happening between vkCmdBeginRendering and vkCmdEndRendering happen after this, but may happen out of order
		{
			VkRenderingAttachmentInfo colorAttachment = attachmentInfo(drawImage.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			VkRenderingInfo renderInfo = renderingInfo(m_swapChainExtent, colorAttachment);
			vkCmdBeginRendering(cmd, &renderInfo);
		}

	}
	{
		// triangle
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
		vkCmdDraw(cmd, 3, 1, 0, 0);
		vkCmdEndDebugUtilsLabelEXT(cmd);
	}

	{
		// end app rendering
		vkCmdEndRendering(cmd);
		{
			// blit the draw image to the swapchain image
			transitionImage(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			transitionImage(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			blitImages(cmd, drawImage.image, image, m_drawExtent, m_swapChainExtent);
		}
		{
			// draw ui onto swapchain image
			transitionImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			result = renderUI(cmd, imageView);
			if (result != VK_SUCCESS) return result;
		}
		{
			// Transition swapchain image for presentation
			transitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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

Rhi::~Rhi() {
	{
		// Wait for everything to be done.
		if (m_device.has_value()) {
			vkDeviceWaitIdle(m_device.value());
		}
	}

	// Deinit begin in the reverse order from how it was created.
	deinitUI();
	for (VkFence fence : m_inFlightFence) {
		vkDestroyFence(m_device.value(), fence, nullptr);
	}
	for (VkSemaphore semaphore : m_imageAvailableSemaphores) {
		vkDestroySemaphore(m_device.value(), semaphore, nullptr);
	}
	for (VkSemaphore semaphore : m_renderFinishedSemaphores) {
		vkDestroySemaphore(m_device.value(), semaphore, nullptr);
	}
	if (m_commandPool.has_value()) {
		vkDestroyCommandPool(m_device.value(), m_commandPool.value(), nullptr);
	}
	if (m_shaderPL.has_value()) {
		vkDestroyPipelineLayout(m_device.value(), m_shaderPL.value(), nullptr);
	}
	for (VkShaderEXT shader : m_shaders) {
		vkDestroyShaderEXT(m_device.value(), shader, nullptr);
	}
	for (VkImageView imageView : m_swapChainImageViews) {
		vkDestroyImageView(m_device.value(), imageView, nullptr);
	}
	if (m_drawImage.has_value()) {
		AllocatedImage drawImage = m_drawImage.value();
		vkDestroyImageView(m_device.value(), drawImage.imageView, nullptr);
		vmaDestroyImage(m_allocator.value(), drawImage.image, drawImage.allocation);
	}
	if (m_swapchain.has_value()) {
		vkDestroySwapchainKHR(m_device.value(), m_swapchain.value(), nullptr);
	}
	if (m_debugMessenger.has_value()) {
		vkDestroyDebugUtilsMessengerEXT(m_instance.value(), m_debugMessenger.value(), nullptr);
	}
	if (m_allocator.has_value()) {
		vmaDestroyAllocator(m_allocator.value());
	}
	if (m_device.has_value()) {
		VkResult result = vkDeviceWaitIdle(m_device.value());
		if (result == VK_SUCCESS) vkDestroyDevice(m_device.value(), NULL);
	}
	if (m_surface.has_value()) {
		SDL_Vulkan_DestroySurface(m_instance.value(), m_surface.value(), nullptr);
	}
	if (m_instance.has_value()) {
		vkDestroyInstance(m_instance.value(), NULL);
	}
}