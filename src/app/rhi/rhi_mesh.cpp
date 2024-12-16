#include "rhi.h"
#include "../loader/loader.h"


namespace {
	constexpr auto ndc = glm::mat4(
		glm::vec4(-1.f, 0.f, 0.f, 0.f),
		glm::vec4(0.f, 1.f, 0.f, 0.f),
		glm::vec4(0.f, 0.f, 1.f, 0.f),
		glm::vec4(0.f, 0.f, 0.f, 1.f)
	);
}

void mesh_scene::init(const rh::ctx& ctx)
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
		rosy_utils::debug_print_a("error reading shader files! %s", e.what());
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
		descriptor_layout_builder layout_builder;
		layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return;
		image_layout = set;
		descriptor_layouts.push_back(set);
	}
	{
		shader_pipeline sp = {};
		sp.layouts = descriptor_layouts;
		sp.name = std::format("scene {}", name);
		sp.with_shaders(scene_vertex_shader, scene_fragment_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return;
		shaders = sp;
	}
}

void mesh_scene::init_shadows(const rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	std::vector<char> scene_vertex_shader;
	std::vector<char> scene_fragment_shader;
	shadow_map_extent_ = ctx.rhi.shadow_map_extent;
	try
	{
		scene_vertex_shader = read_file(shadow_vertex_path);
		scene_fragment_shader = read_file(shadow_frag_path);
	}
	catch (const std::exception& e)
	{
		rosy_utils::debug_print_a("error reading shadow shader files! %s", e.what());
		return;
	}

	{
		shadow_descriptor_layouts.push_back(data_layout);
	}

	{
		shader_pipeline sp = {};
		sp.layouts = shadow_descriptor_layouts;
		sp.name = std::format("shadows {}", name);
		sp.with_shaders(scene_vertex_shader, scene_fragment_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return;
		shadow_shaders = sp;
	}
}

void mesh_scene::deinit(const rh::ctx& ctx)
{
	const auto buffer = ctx.rhi.data.value();
	const VkDevice device = ctx.rhi.device;
	for (std::shared_ptr<mesh_asset> mesh : meshes)
	{
		gpu_mesh_buffers rectangle = mesh.get()->mesh_buffers;
		buffer->destroy_buffer(rectangle.vertex_buffer);
		buffer->destroy_buffer(rectangle.index_buffer);
		mesh.reset();
	}
	for (ktx_auto_texture tx : ktx_textures)
	{
		ctx.rhi.data.value()->destroy_image(tx);
	}
	for (const VkImageView iv : image_views)
	{
		vkDestroyImageView(device, iv, nullptr);
	}
	for (const VkSampler smp : samplers)
	{
		vkDestroySampler(device, smp, nullptr);
	}
	for (const VkDescriptorSetLayout set : descriptor_layouts)
	{
		vkDestroyDescriptorSetLayout(device, set, nullptr);
	}
	if (descriptor_allocator.has_value()) {
		descriptor_allocator.value().destroy_pools(device);
	}
	if (shadow_shaders.has_value()) {
		shadow_shaders.value().deinit(device);
	}
	if (shaders.has_value()) {
		shaders.value().deinit(device);
	}
}

void mesh_scene::add_node(fastgltf::Node& gltf_node)
{
	const auto new_node = std::make_shared<mesh_node>();
	if (gltf_node.meshIndex.has_value())
	{
		new_node->mesh_index = gltf_node.meshIndex.value();

		auto [translation, rotation, scale] = std::get<fastgltf::TRS>(gltf_node.transform);

		const auto tr = glm::vec3(translation[0], translation[1], translation[2]);
		const auto ro = glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]);
		const auto sc = glm::vec3(scale[0], scale[1], scale[2]);

		const auto tm = translate(glm::mat4(1.0f), tr);
		const auto rm = toMat4(ro);
		const auto sm = glm::scale(glm::mat4(1.0f), sc);

		new_node->model_transform = tm * rm * sm;
	}
	new_node->children.reserve(gltf_node.children.size());
	for (size_t i : gltf_node.children)	new_node->children.emplace_back(i);
	nodes.push_back(new_node);
};

void mesh_scene::add_scene(fastgltf::Scene& gltf_scene)
{
	scenes.emplace_back(gltf_scene.nodeIndices.begin(), gltf_scene.nodeIndices.end());
};

[[nodiscard]] std::vector<render_object> mesh_scene::draw_queue(const size_t scene_index, const glm::mat4& m) const
{
	if (scene_index >= scenes.size()) return {};
	std::queue<std::shared_ptr<mesh_node>> queue{};
	std::vector<render_object> draw_nodes{};
	for (const size_t node_index : scenes[scene_index])
	{
		auto draw_node = nodes[node_index];
		draw_node->update(m, nodes);
		queue.push(draw_node);

	}
	while (queue.size() > 0)
	{
		const auto current_node = queue.front();
		queue.pop();
		if (current_node->mesh_index.has_value())
		{
			for (
				const std::shared_ptr<mesh_asset> ma = meshes[current_node->mesh_index.value()];
				const auto [start_index, count, material] : ma->surfaces)
			{
				render_object ro{};
				ro.transform = current_node->world_transform;
				ro.first_index = start_index;
				ro.index_count = count;
				ro.index_buffer = ma->mesh_buffers.index_buffer.buffer;
				ro.vertex_buffer_address = ma->mesh_buffers.vertex_buffer_address;
				ro.material_index = material;
				draw_nodes.push_back(ro);
			}
		}
		for (const size_t child_index : current_node->children)
		{
			queue.push(nodes[child_index]);
		}
	}
	return draw_nodes;
}

rh::result mesh_scene::draw(mesh_ctx ctx)
{
	if (meshes.size() == 0) return rh::result::ok;
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
		m_shaders.shader_constants_size = sizeof(gpu_draw_push_constants);
		m_shaders.front_face = ctx.front_face;
		if (VkResult result = m_shaders.shade(cmd); result != VK_SUCCESS) return rh::result::error;
	}

	size_t last_material = 100'000;

	for (auto ro : draw_queue(ctx.scene_index, ndc * ctx.world_transform)) {
		if (ro.material_index != last_material)
		{
			last_material = ro.material_index;
			auto [pass_type, descriptor_set_id] = materials[ro.material_index];
			VkDescriptorSet desc = descriptor_sets[descriptor_set_id];
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shaders.pipeline_layout.value(), 0, 1, ctx.global_descriptor, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shaders.pipeline_layout.value(), 1, 1, &desc, 0, nullptr);
		}
		gpu_draw_push_constants push_constants{};
		push_constants.world_matrix = ro.transform;
		push_constants.vertex_buffer = ro.vertex_buffer_address;
		m_shaders.shader_constants = &push_constants;
		m_shaders.shader_constants_size = sizeof(push_constants);
		if (VkResult result = m_shaders.push(cmd); result != VK_SUCCESS) return rh::result::error;
		vkCmdBindIndexBuffer(cmd, ro.index_buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, ro.index_count, 1, ro.first_index, 0, 0);
	}

	vkCmdEndDebugUtilsLabelEXT(cmd);
	return rh::result::ok;
};

rh::result mesh_scene::generate_shadows(mesh_ctx ctx)
{
	if (meshes.size() == 0) return rh::result::ok;
	auto cmd = ctx.cmd;

	{
		VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label(name.c_str(), color);
		vkCmdBeginDebugUtilsLabelEXT(cmd, &mesh_draw_label);
	}

	shader_pipeline m_shaders = shadow_shaders.value();
	{
		m_shaders.viewport_extent = shadow_map_extent_;
		m_shaders.wire_frames_enabled = false;
		m_shaders.depth_enabled = true;
		m_shaders.shader_constants_size = sizeof(gpu_draw_push_constants);
		m_shaders.front_face = ctx.front_face;
		if (VkResult result = m_shaders.shade(cmd); result != VK_SUCCESS) return rh::result::error;
	}

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shaders.pipeline_layout.value(), 0, 1, ctx.global_descriptor, 0, nullptr);
	for (auto ro : draw_queue(ctx.scene_index, ndc * ctx.world_transform)) {
		gpu_draw_push_constants push_constants{};
		push_constants.world_matrix = ro.transform;
		push_constants.vertex_buffer = ro.vertex_buffer_address;
		m_shaders.shader_constants = &push_constants;
		m_shaders.shader_constants_size = sizeof(push_constants);
		if (VkResult result = m_shaders.push(cmd); result != VK_SUCCESS) return rh::result::error;
		vkCmdBindIndexBuffer(cmd, ro.index_buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, ro.index_count, 1, ro.first_index, 0, 0);
	}

	vkCmdEndDebugUtilsLabelEXT(cmd);
	return rh::result::ok;
};

