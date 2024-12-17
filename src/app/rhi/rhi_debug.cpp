#include "rhi.h"
#include "../loader/loader.h"

void debug_gfx::init(const rh::ctx& ctx)
{
	std::vector<char> scene_vertex_shader;
	std::vector<char> scene_fragment_shader;
	try
	{
		scene_vertex_shader = read_file(vertex_path);
		scene_fragment_shader = read_file(frag_path);
	}
	catch (const std::exception& e)
	{
		rosy_utils::debug_print_a("error reading debug shader files! %s", e.what());
		return;
	}

	const VkDevice device = ctx.rhi.device;
	std::vector<descriptor_allocator_growable::pool_size_ratio> frame_sizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
	};

	descriptor_allocator = descriptor_allocator_growable{};
	descriptor_allocator.value().init(device, 1000, frame_sizes);

	{
		descriptor_layout_builder layout_builder;
		layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return;
		data_layout = set;
		descriptor_layouts.push_back(set);
	}
	{
		shader_pipeline sp = {};
		sp.layouts = descriptor_layouts;
		sp.name = std::format("scene {}", name);
		sp.shader_constants_size = sizeof(debug_draw_push_constants);
		sp.with_shaders(scene_vertex_shader, scene_fragment_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return;
		shaders = sp;
	}
}

void debug_gfx::deinit(const rh::ctx& ctx)
{
	const auto buffer = ctx.rhi.data.value();
	const VkDevice device = ctx.rhi.device;
	for (const VkDescriptorSetLayout set : descriptor_layouts)
	{
		vkDestroyDescriptorSetLayout(device, set, nullptr);
	}
	if (descriptor_allocator.has_value()) {
		descriptor_allocator.value().destroy_pools(device);
	}
	if (shaders.has_value()) {
		shaders.value().deinit(device);
	}
}

rh::result debug_gfx::draw(debug_ctx ctx)
{
	if (ctx.lines.size() == 0) return rh::result::ok;
	auto cmd = ctx.cmd;

	{
		VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label(name.c_str(), color);
		vkCmdBeginDebugUtilsLabelEXT(cmd, &mesh_draw_label);
	}

	shader_pipeline m_shaders = shaders.value();
	{
		m_shaders.viewport_extent = ctx.extent;
		m_shaders.wire_frames_enabled = ctx.wire_frame;
		m_shaders.depth_enabled = ctx.depth_enabled;
		m_shaders.front_face = ctx.front_face;
		if (VkResult result = m_shaders.shade(cmd); result != VK_SUCCESS) return rh::result::error;
	}

	size_t last_material = 100'000;

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shaders.pipeline_layout.value(), 0, 1, ctx.global_descriptor, 0, nullptr);
	for (auto line : ctx.lines) {
		gpu_draw_push_constants push_constants{};
		m_shaders.shader_constants = &line;
		m_shaders.shader_constants_size = sizeof(line);
		if (VkResult result = m_shaders.push(cmd); result != VK_SUCCESS) return rh::result::error;
		vkCmdDrawIndexed(cmd, 2, 1, 0, 0, 0);
	}

	vkCmdEndDebugUtilsLabelEXT(cmd);
	return rh::result::ok;
};

