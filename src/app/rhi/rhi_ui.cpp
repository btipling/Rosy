
#include "rhi.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

VkResult rhi::init_ui(SDL_Window* window) {
	const VkDescriptorPoolSize pool_sizes[] = { 
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

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imgui_pool{};
	const VkDevice device = opt_device.value();
	if (const VkResult result = vkCreateDescriptorPool(device, &pool_info, nullptr, &imgui_pool); result != VK_SUCCESS) return result;

	ImGui::CreateContext();
	ImGui_ImplSDL3_InitForVulkan(window);

	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.Instance = instance_.value();
	init_info.PhysicalDevice = physical_device_.value();
	init_info.Device = opt_device.value();
	init_info.Queue = present_queue_.value();
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.UseDynamicRendering = true;

	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchain_image_format_.format;


	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);
	ImGui_ImplVulkan_CreateFontsTexture();

	int displays_count = 0; // TODO: don't always get the first display
	const auto display_ids = SDL_GetDisplays(&displays_count);
	const float content_scale = SDL_GetDisplayContentScale(*display_ids);

	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = content_scale;

	ui_pool_ = imgui_pool;
	return VK_SUCCESS;
}

VkResult rhi::render_ui(const VkCommandBuffer cmd, const VkImageView target_image_view) const
{
	const VkRenderingAttachmentInfo color_attachment = rhi_helpers::attachment_info(target_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const VkRenderingInfo render_info = rhi_helpers::rendering_info(swapchain_extent, color_attachment, std::nullopt);
	vkCmdBeginRendering(cmd, &render_info);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
	return VK_SUCCESS;
}

void rhi::deinit_ui() const
{
	if (ui_pool_.has_value() && ui_pool_.value()) {
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
		vkDestroyDescriptorPool(opt_device.value(), ui_pool_.value(), nullptr);
	}
}
