
#include <array>
#include "RHI.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

VkResult Rhi::initUI(SDL_Window* window) {
	VkDescriptorPoolSize poolSizes[] = { 
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = (uint32_t)std::size(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	VkDescriptorPool imguiPool;
	VkDevice device = m_device.value();
	VkResult result;
	result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiPool);
	if (result != VK_SUCCESS) return result;

	ImGui::CreateContext();
	ImGui_ImplSDL3_InitForVulkan(window);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_instance.value();
	init_info.PhysicalDevice = m_physicalDevice.value();
	init_info.Device = m_device.value();
	init_info.Queue = m_presentQueue.value();
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.UseDynamicRendering = true;

	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_swapChainImageFormat;


	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);
	ImGui_ImplVulkan_CreateFontsTexture();

	int displaysCount = 0; // TODO: don't always get the first display
	auto displayIds = SDL_GetDisplays(&displaysCount);
	float contentScale = SDL_GetDisplayContentScale(*displayIds);

	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = contentScale;

	m_uiPool = imguiPool;
	return VK_SUCCESS;
}

VkResult Rhi::renderUI(VkCommandBuffer cmd, VkImageView targetImageView) {
	VkResult result;

	VkRenderingAttachmentInfo colorAttachment = attachmentInfo(targetImageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = renderingInfo(m_swapChainExtent, colorAttachment, std::nullopt);
	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
	return VK_SUCCESS;
}

VkResult Rhi::drawUI() {
	ImGui::SliderFloat("Rotate X", &m_model_rot_x, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Rotate Y", &m_model_rot_y, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Rotate Z", &m_model_rot_z, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Translate X", &m_model_x, -1000.0f, 1000.0f);
	ImGui::SliderFloat("Translate Y", &m_model_y, -1000.0f, 1000.0f);
	ImGui::SliderFloat("Translate Z", &m_model_z, -1000.0f, 10.0f);
	ImGui::SliderFloat("Scale", &m_model_scale, 0.1f, 10.0f);
	ImGui::SliderFloat("Perspective Depth", &m_perspective_d, 100.0f, 20000.0f);
	return VK_SUCCESS;
}

void Rhi::deinitUI() {
	if (m_uiPool.value()) {
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
		vkDestroyDescriptorPool(m_device.value(), m_uiPool.value(), nullptr);
	}
}