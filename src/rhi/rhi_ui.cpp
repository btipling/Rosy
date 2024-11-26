
#include <array>
#include "RHI.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

VkResult rhi::initUI(SDL_Window* window) {
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
	VkDevice device = device_.value();
	VkResult result;
	result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiPool);
	if (result != VK_SUCCESS) return result;

	ImGui::CreateContext();
	ImGui_ImplSDL3_InitForVulkan(window);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = instance_.value();
	init_info.PhysicalDevice = physical_device_.value();
	init_info.Device = device_.value();
	init_info.Queue = present_queue_.value();
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.UseDynamicRendering = true;

	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain_image_format_.format;


	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);
	ImGui_ImplVulkan_CreateFontsTexture();

	int displaysCount = 0; // TODO: don't always get the first display
	auto displayIds = SDL_GetDisplays(&displaysCount);
	float contentScale = SDL_GetDisplayContentScale(*displayIds);

	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = contentScale;

	ui_pool_ = imguiPool;
	return VK_SUCCESS;
}

VkResult rhi::renderUI(VkCommandBuffer cmd, VkImageView targetImageView) {
	VkResult result;

	VkRenderingAttachmentInfo colorAttachment = attachmentInfo(targetImageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = renderingInfo(swapchain_extent_, colorAttachment, std::nullopt);
	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
	return VK_SUCCESS;
}

VkResult rhi::draw_ui() {
	ImGui::SliderFloat("Rotate X", &model_rot_x_, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Rotate Y", &model_rot_y_, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Rotate Z", &model_rot_z_, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Translate X", &model_x_, -100.0f, 100.0f);
	ImGui::SliderFloat("Translate Y", &model_y_, -100.0f, 100.0f);
	ImGui::SliderFloat("Translate Z", &model_z_, -1000.0f, 10.0f);
	ImGui::SliderFloat("Scale", &model_scale_, 0.1f, 10.0f);
	ImGui::Checkbox("Wireframe", &toggle_wire_frame_);
	ImGui::Text("Blending");
	ImGui::RadioButton("disabled", &blend_mode_, 0); ImGui::SameLine();
	ImGui::RadioButton("additive", &blend_mode_, 1); ImGui::SameLine();
	ImGui::RadioButton("alpha blend", &blend_mode_, 2);
	ImGui::SliderFloat("Render Scale", &render_scale_, 0.3f, 1.f);
	return VK_SUCCESS;
}

void rhi::deinitUI() {
	if (ui_pool_.value()) {
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
		vkDestroyDescriptorPool(device_.value(), ui_pool_.value(), nullptr);
	}
}