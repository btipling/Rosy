#include "rhi.h"
#include "../loader/loader.h"


namespace {
	constexpr auto ndc = glm::mat4(
		glm::vec4(-1.f, 0.f, 0.f, 0.f),
		glm::vec4(0.f, 1.f, 0.f, 0.f),
		glm::vec4(0.f, 0.f, 1.f, 0.f),
		glm::vec4(0.f, 0.f, 0.f, 1.f)
	);

	gpu_draw_push_constants push_constants(const render_object& ro)
	{
		gpu_draw_push_constants push_constants{};
		push_constants.scene_buffer = ro.scene_buffer_address;
		push_constants.vertex_buffer = ro.vertex_buffer_address;
		push_constants.render_buffer = ro.render_buffer_address + (sizeof(render_data) * ro.mesh_index);
		push_constants.material_buffer = ro.material_buffer_address + (sizeof(material_data) * ro.material_index);
		return push_constants;
	}
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

	std::vector<VkDescriptorSetLayout>layouts{};
	const VkDevice device = ctx.rhi.device;
	{
		layouts.push_back(ctx.rhi.descriptor_sets.value()->descriptor_set_layout.value());
		shader_pipeline sp = {};
		sp.layouts = layouts;
		sp.name = std::format("scene {}", name);
		sp.with_shaders(scene_vertex_shader, scene_fragment_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return;
		shaders = sp;
	}
	{
		debug_gfx db{};
		debug = std::make_shared<debug_gfx>(std::move(db));
		debug->init(ctx);
		constexpr float label[4] = { 0.5f, 0.1f, 0.1f, 1.0f };
		debug->name = std::format("{} debug", name);
		std::ranges::copy(label, std::begin(color));
		{
			// Cross
			debug_draw_push_constants line{};
			line.world_matrix = glm::mat4(1.f);
			line.color = glm::vec4(1.f, 0.f, 0.f, 1.f);
			line.p1 = glm::vec4(0.f, 0.f, 0.f, 1.f);
			line.p2 = glm::vec4(1.f, 0.f, 0.f, 1.f);
			debug->lines.push_back(line);
			line.p1 = glm::vec4(0.f, 0.f, 0.f, 1.f);
			line.p2 = glm::vec4(0.f, 1.f, 0.f, 1.f);
			line.color = glm::vec4(0.f, 1.f, 0.f, 1.f);
			debug->lines.push_back(line);
			line.p1 = glm::vec4(0.f, 0.f, 0.f, 1.f);
			line.p2 = glm::vec4(0.f, 0.f, 1.f, 1.f);
			line.color = glm::vec4(0.f, 0.f, 1.f, 1.f);
			debug->lines.push_back(line);
		}
	}
	{
		const auto [result, new_scene_buffers] = ctx.rhi.data.value()->create_scene_data();
		if (result != VK_SUCCESS)
		{
			rosy_utils::debug_print_a("Failed to init scene buffer %d\n", result);
			return;
		}
		scene_buffers = new_scene_buffers;
	}
}

void mesh_scene::deinit(const rh::ctx& ctx) const
{
	const auto buffer = ctx.rhi.data.value();
	const VkDevice device = ctx.rhi.device;
	{
		debug->deinit(ctx);
	}
	if (scene_buffers.has_value()) {
		buffer->destroy_buffer(scene_buffers.value().scene_buffer);
	}
	if (render_buffers.has_value()) {
		buffer->destroy_buffer(render_buffers.value().render_buffer);
	}
	if (material_buffers.has_value()) {
		buffer->destroy_buffer(material_buffers.value().material_buffer);
	}
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
	if (shadow_shaders.has_value()) {
		shadow_shaders.value().deinit(device);
	}
	if (shaders.has_value()) {
		shaders.value().deinit(device);
	}
}

void mesh_scene::init_shadows(const rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	std::vector<char> scene_vertex_shader;
	shadow_map_extent_ = ctx.rhi.shadow_map_extent;
	try
	{
		scene_vertex_shader = read_file(shadow_vertex_path);
	}
	catch (const std::exception& e)
	{
		rosy_utils::debug_print_a("error reading shadow shader files! %s", e.what());
		return;
	}

	{
		shader_pipeline sp = {};
		sp.name = std::format("shadows {}", name);
		sp.with_shaders(scene_vertex_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return;
		shadow_shaders = sp;
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

void mesh_scene::update(mesh_ctx ctx, std::optional<gpu_scene_data> scene_data)
{
	if (ctx.scene_index >= scenes.size()) return;
	if (scene_data.has_value() && scene_buffers.has_value()) {
		gpu_scene_buffers sb = scene_buffers.value();
		gpu_scene_data sd = scene_data.value();
		memcpy(sb.scene_buffer.info.pMappedData, &sd, sizeof(sd));
	}

	std::queue<std::shared_ptr<mesh_node>> queue{};
	draw_nodes_.clear();
	std::vector<render_data> render_datas;
	for (const size_t node_index : scenes[ctx.scene_index])
	{
		auto draw_node = nodes[node_index];
		draw_node->update(ndc * ctx.world_transform, nodes);
		queue.push(draw_node);

	}
	size_t mesh_index = 0;
	while (queue.size() > 0)
	{
		const auto current_node = queue.front();
		queue.pop();
		if (current_node->mesh_index.has_value())
		{
			for (
				const std::shared_ptr<mesh_asset> ma = meshes[current_node->mesh_index.value()];
				const auto [start_index, count, material] : ma->surfaces
			)
			{
				render_data rd{};
				rd.transform = current_node->world_transform;
				rd.normal_transform = glm::transpose(glm::inverse(static_cast<glm::mat3>(current_node->world_transform)));
				rd.material_data = glm::uvec4(static_cast<glm::uint>(material), 0, 0, 0);
				render_datas.push_back(rd);

				render_object ro{};
				ro.transform = current_node->world_transform;
				ro.first_index = start_index;
				ro.index_count = count;
				ro.index_buffer = ma->mesh_buffers.index_buffer.buffer;
				ro.scene_buffer_address = scene_buffers.value().scene_buffer_address;
				ro.vertex_buffer_address = ma->mesh_buffers.vertex_buffer_address;
				ro.render_buffer_address = render_buffers.value().render_buffer_address;
				if (material_buffers.has_value()) {
					ro.material_buffer_address = material_buffers.value().material_buffer_address;
				}
				ro.material_index = material;
				ro.mesh_index = mesh_index;
				draw_nodes_.push_back(ro);
				mesh_index += 1;
			}
		}
		for (const size_t child_index : current_node->children)
		{
			queue.push(nodes[child_index]);
		}
	}
	{
		const gpu_render_buffers rb = render_buffers.value();
		const size_t render_buffer_size = render_datas.size() * sizeof(render_data);
		assert(render_buffer_size <= rb.buffer_size);
		memcpy(rb.render_buffer.info.pMappedData, render_datas.data(), render_buffer_size);
	}

}

rh::result mesh_scene::draw(mesh_ctx ctx)
{
	if (meshes.size() == 0) return rh::result::ok;
	auto render_cmd = ctx.render_cmd;

	{
		VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label(name.c_str(), color);
		vkCmdBeginDebugUtilsLabelEXT(render_cmd, &mesh_draw_label);
	}

	shader_pipeline m_shaders = shaders.value();
	{
		m_shaders.viewport_extent = ctx.extent;
		m_shaders.wire_frames_enabled = ctx.wire_frame;
		m_shaders.depth_enabled = ctx.depth_enabled;
		m_shaders.shader_constants_size = sizeof(gpu_draw_push_constants);
		m_shaders.front_face = ctx.front_face;
		if (VkResult result = m_shaders.shade(render_cmd); result != VK_SUCCESS) return rh::result::error;
	}

	{
		VkDescriptorSet desc = ctx.ctx->rhi.descriptor_sets.value()->descriptor_set.value();
		vkCmdBindDescriptorSets(render_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shaders.pipeline_layout.value(), 0, 1, &desc, 0, nullptr);
	}

	for (auto ro : draw_nodes_) {
		gpu_draw_push_constants pc = push_constants(ro);
		m_shaders.shader_constants = &pc;
		if (VkResult result = m_shaders.push(render_cmd); result != VK_SUCCESS) return rh::result::error;
		vkCmdBindIndexBuffer(render_cmd, ro.index_buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(render_cmd, ro.index_count, 1, ro.first_index, 0, 0);
	}

	vkCmdEndDebugUtilsLabelEXT(render_cmd);


	// Debug
	if (scene_buffers.has_value()) {
		if (const auto res = debug->draw(ctx, scene_buffers.value().scene_buffer_address); res != rh::result::ok) return res;
	}
	return rh::result::ok;
};

rh::result mesh_scene::generate_shadows(mesh_ctx ctx, int pass_number)
{
	if (meshes.size() == 0) return rh::result::ok;
	auto mv_cmd = ctx.shadow_pass_cmd;

	shader_pipeline m_shaders = shadow_shaders.value();
	if (pass_number == 0) {

		VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label(name.c_str(), color);
		vkCmdBeginDebugUtilsLabelEXT(mv_cmd, &mesh_draw_label);

		m_shaders.viewport_extent = shadow_map_extent_;
		m_shaders.wire_frames_enabled = false;
		m_shaders.depth_enabled = true;
		m_shaders.culling_enabled = false;
		m_shaders.front_face = ctx.front_face;
		if (VkResult result = m_shaders.shade(mv_cmd); result != VK_SUCCESS) return rh::result::error;
	}

	for (auto ro : draw_nodes_) {
		gpu_draw_push_constants pc = push_constants(ro);
		m_shaders.shader_constants = &pc;
		m_shaders.shader_constants_size = sizeof(pc);
		if (VkResult result = m_shaders.push(mv_cmd); result != VK_SUCCESS) return rh::result::error;
		vkCmdBindIndexBuffer(mv_cmd, ro.index_buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(mv_cmd, ro.index_count, 1, ro.first_index, 0, 0);
	}

	if (pass_number == 2) vkCmdEndDebugUtilsLabelEXT(mv_cmd);
	return rh::result::ok;
};

