#include <numbers>

#include "imgui.h"
#include "rhi.h"
#include "../loader/loader.h"

namespace {
	constexpr auto gltf_to_ndc = glm::mat4(
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

	gpu_shadow_push_constants shadow_constants(const render_object& ro, const int pass_number)
	{
		gpu_shadow_push_constants shadow_constants{};
		shadow_constants.scene_buffer = ro.scene_buffer_address;
		shadow_constants.vertex_buffer = ro.vertex_buffer_address;
		shadow_constants.render_buffer = ro.render_buffer_address + (sizeof(render_data) * ro.mesh_index);
		shadow_constants.pass_number = static_cast<glm::uint32>(pass_number);
		return shadow_constants;
	}
}

void mesh_scene::init(const rh::ctx& ctx)
{
	ZoneScopedNC("init_mesh", 0xB19CD8);
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
	ZoneScopedNC("deinit_mesh", 0xE6D1F2);
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
	ZoneScopedNC("init_shadows", 0xBEA9DF);
	auto cam = camera(glm::vec3{ 3.75f, 4.32f, 2.84f });
	cam.pitch = 0.36f;
	cam.yaw = 1.f;
	mesh_cam = std::make_unique<camera>(std::move(cam));
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
		auto m = glm::mat4{ 1.f };
		m = glm::rotate(m, sunlight_x_rot_, glm::vec3(1.f, 0.f, 0.f));
		m = glm::rotate(m, sunlight_y_rot_, glm::vec3(0.f, 1.f, 0.f));
		m = glm::rotate(m, sunlight_z_rot_, glm::vec3(0.f, 0.f, 1.f));
		light_transform_ = m;
	}
	{
		shader_pipeline sp = {};
		sp.name = std::format("shadows {}", name);
		sp.with_shaders(scene_vertex_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return;
		shadow_shaders = sp;
		debug->set_sunlight(light_transform_);
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
}

glm::mat4 mesh_scene::csm_pos(const int csm_extent)
{
	float cascade_level = 50.f;
	if (csm_extent == 1)
	{
		cascade_level = 150.f;
	}
	if (csm_extent == 2)
	{
		cascade_level = 300.f;
	}
	const auto shadow_p = glm::mat4(
		glm::vec4(2.f / cascade_level, 0.f, 0.f, 0.f),
		glm::vec4(0.f, 2.f / cascade_level, 0.f, 0.f),
		glm::vec4(0.f, 0.f, 1.f / 500.f, 0.f),
		glm::vec4(0.f, 0.f, 0.f, 1.f)
	);
	const glm::mat4 l_light = light_transform_;
	glm::vec3 l_pos = mesh_cam->position;
	l_pos[1] -= 190;
	l_pos[2] += 35;
	const auto l = glm::mat4(
		l_light[0],
		l_light[1],
		l_light[2],
		glm::vec4(l_pos, 1.f)
	);
	return shadow_p * glm::inverse(l);
}

void mesh_scene::draw_ui(const rh::ctx& ctx)
{
	ZoneScopedNC("draw_ui", 0xBEA9DF);
	const glm::mat4 L = light_transform_;
	ImGui::Begin("Sunlight & Shadow");
	{
		bool rotate = false;
		ImGui::Text("Light position: (%.3f, %.3f, %.3f)", light_pos_.x, light_pos_.y, light_pos_.z);
		ImGui::Text("Light direction: (%.3f, %.3f, %.3f)", L[2][0], L[2][1], L[2][2]);
		ImGui::Text("CSM dimension %.3f", csm_dk_);

		ImGui::RadioButton("near", &csm_extent_, 0); ImGui::SameLine();
		ImGui::RadioButton("middle", &csm_extent_, 1); ImGui::SameLine();
		ImGui::RadioButton("far", &csm_extent_, 2);

		ImGui::Text("Light View");
		static int elem = static_cast<int>(current_view_);
		const char* elems_names[3] = { "Camera", "CSM", "Light" };
		const char* elem_name = (elem >= 0 && elem < 3) ? elems_names[elem] : "Unknown";
		current_view_ = static_cast<camera_view>(elem);
		ImGui::SliderInt("slider enum", &elem, 0, 3 - 1, elem_name);

		if (ImGui::SliderFloat("Sunlight rotation x", &sunlight_x_rot_, 0, std::numbers::pi * 2.f, "%.3f")) {
			auto m = glm::mat4{ 1.f };
			m = glm::rotate(m, sunlight_x_rot_, glm::vec3(1.f, 0.f, 0.f));
			m = glm::rotate(m, sunlight_y_rot_, glm::vec3(0.f, 1.f, 0.f));
			m = glm::rotate(m, sunlight_z_rot_, glm::vec3(0.f, 0.f, 1.f));
			 light_transform_ = m;
		}
		if (ImGui::SliderFloat("Sunlight rotation y", &sunlight_y_rot_, 0, std::numbers::pi * 2.f, "%.3f")) {
			auto m = glm::mat4{ 1.f };
			m = glm::rotate(m, sunlight_x_rot_, glm::vec3(1.f, 0.f, 0.f));
			m = glm::rotate(m, sunlight_y_rot_, glm::vec3(0.f, 1.f, 0.f));
			m = glm::rotate(m, sunlight_z_rot_, glm::vec3(0.f, 0.f, 1.f));
			light_transform_ = m;
		}
		if (ImGui::SliderFloat("Sunlight rotation z", &sunlight_z_rot_, 0, std::numbers::pi * 2.f, "%.3f")) {
			auto m = glm::mat4{ 1.f };
			m = glm::rotate(m, sunlight_x_rot_, glm::vec3(1.f, 0.f, 0.f));
			m = glm::rotate(m, sunlight_y_rot_, glm::vec3(0.f, 1.f, 0.f));
			m = glm::rotate(m, sunlight_z_rot_, glm::vec3(0.f, 0.f, 1.f));
			light_transform_ = m;
		}
		ImGui::SliderFloat("Cascade factor", &cascade_factor_, 0, 1, "%.3f");
		ImGui::Text("Depth bias");
		ImGui::Checkbox("Enabled", &depth_bias_enabled);
		ImGui::SliderFloat("constant", &depth_bias_constant, -500.f, 500.f);
		ImGui::SliderFloat("clamp", &depth_bias_clamp, -500.f, 500.f);
		ImGui::SliderFloat("slope factor", &depth_bias_slope_factor, -500.f, 500.f);
	}
	ImGui::End();
};


gpu_scene_data mesh_scene::scene_update(const rh::ctx& ctx)
{
	{
		mesh_cam->process_sdl_event(ctx);
		mesh_cam->update(ctx);
	}
	{
		constexpr auto ndc = glm::mat4(
			glm::vec4(1.f, 0.f, 0.f, 0.f),
			glm::vec4(0.f, -1.f, 0.f, 0.f),
			glm::vec4(0.f, 0.f, 1.f, 0.f),
			glm::vec4(0.f, 0.f, 0.f, 1.f)
		);

		constexpr auto light_view_to_ndc = glm::mat4(
			glm::vec4(1.f, 0.f, 0.f, 0.f),
			glm::vec4(0.f, 1.f, 0.f, 0.f),
			glm::vec4(0.f, 0.f, 1.f, 0.f),
			glm::vec4(0.f, 0.f, 0.f, 1.f)
		);

		const auto [width, height] = ctx.rhi.frame_extent;
		constexpr float z_near = 0.1f;
		constexpr float z_far = 1000.0f;
		const float aspect = static_cast<float>(width) / static_cast<float>(height);
		constexpr float fov = glm::radians(70.0f);
		const float h = 1.0 / tan(fov * 0.5);
		const float w = h / aspect;
		constexpr float a = -z_near / (z_far - z_near);
		constexpr float b = (z_near * z_far) / (z_far - z_near);


		glm::mat4 proj(
			glm::vec4(w, 0, 0, 0),
			glm::vec4(0, h, 0, 0),
			glm::vec4(0, 0, a, 1.f),
			glm::vec4(0, 0, b, 0));

		proj = ndc * proj;

		const glm::mat4 view = mesh_cam->get_view_matrix();

		const glm::mat4 new_sunlight = light_transform_;
		glm::mat4 p{ proj * view };

		switch (current_view_)
		{
		case camera_view::csm:
			p = csm_pos(csm_extent_);
			break;
		case camera_view::light:
			p = light_view_to_ndc * proj * glm::inverse(new_sunlight);
			break;
		default:
			p = proj * view;
			break;
		}

		scene_data.view = view;
		scene_data.proj = proj;
		scene_data.view_projection = p;

		//scene_data.shadow_projection_near = sm_projection_near * sm_view_near;
		//scene_data.shadow_projection_middle = sm_projection_middle * sm_view_middle;
		//scene_data.shadow_projection_far = sm_projection_far * sm_view_far;
		scene_data.shadow_projection_near = csm_pos(0);
		scene_data.shadow_projection_middle = csm_pos(1);
		scene_data.shadow_projection_far = csm_pos(2);

		scene_data.camera_position = glm::vec4(mesh_cam->position, 1.f);
		scene_data.ambient_color = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
		scene_data.sunlight = new_sunlight;
		scene_data.sunlight_color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);

	}
	return scene_data;
}

void mesh_scene::update(mesh_ctx ctx, std::optional<gpu_scene_data> scene_data)
{
	if (ctx.scene_index >= scenes.size()) return;
	if (scene_data.has_value() && scene_buffers.has_value()) {
		gpu_scene_buffers sb = scene_buffers.value();
		gpu_scene_data sd = scene_data.value();
		sd.csm_index_sampler = ctx.ctx->rhi.csm_index_sampler;
		sd.csm_index_near = ctx.ctx->rhi.csm_index_near;
		sd.csm_index_middle = ctx.ctx->rhi.csm_index_middle;
		sd.csm_index_far = ctx.ctx->rhi.csm_index_far;
		memcpy(sb.scene_buffer.info.pMappedData, &sd, sizeof(sd));
	}

	std::queue<std::shared_ptr<mesh_node>> queue{};
	draw_nodes_.clear();
	std::vector<render_data> render_datas;
	for (const size_t node_index : scenes[ctx.scene_index])
	{
		auto draw_node = nodes[node_index];
		draw_node->update(gltf_to_ndc * ctx.world_transform, nodes);
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
	ZoneScopedNC("mesh_draw", 0xCCB7E5);
	if (meshes.size() == 0) return rh::result::ok;
	auto render_cmd = ctx.render_cmd;
	{
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
			ZoneScopedNC("draw_nodes_", 0xF9FFD1);
			gpu_draw_push_constants pc = push_constants(ro);
			m_shaders.shader_constants = &pc;
			if (VkResult result = m_shaders.push(render_cmd); result != VK_SUCCESS) return rh::result::error;
			vkCmdBindIndexBuffer(render_cmd, ro.index_buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(render_cmd, ro.index_count, 1, ro.first_index, 0, 0);
		}

		vkCmdEndDebugUtilsLabelEXT(render_cmd);
	}

	// Debug
	if (scene_buffers.has_value()) {
		if (const auto res = debug->draw(ctx, scene_buffers.value().scene_buffer_address); res != rh::result::ok) return res;
	}
	return rh::result::ok;
};

rh::result mesh_scene::generate_shadows(mesh_ctx ctx, int pass_number)
{
	ZoneScopedNC("mesh_generate_shadows", 0xD9C4EC);
	if (meshes.size() == 0) return rh::result::ok;
	auto mv_cmd = ctx.shadow_pass_cmd;
	{
		shader_pipeline m_shaders = shadow_shaders.value();
		if (pass_number == 0) {
			VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label(name.c_str(), color);
			vkCmdBeginDebugUtilsLabelEXT(mv_cmd, &mesh_draw_label);
			if (depth_bias_enabled) {
				vkCmdSetDepthBiasEnable(mv_cmd, depth_bias_enabled);
				vkCmdSetDepthClampEnableEXT(mv_cmd, true);
				vkCmdSetDepthBias(mv_cmd, depth_bias_constant, depth_bias_clamp, depth_bias_slope_factor);
			}
			m_shaders.viewport_extent = shadow_map_extent_;
			m_shaders.wire_frames_enabled = false;
			m_shaders.depth_enabled = true;
			m_shaders.culling_enabled = false;
			m_shaders.front_face = ctx.front_face;
			if (VkResult result = m_shaders.shade(mv_cmd); result != VK_SUCCESS) return rh::result::error;
		}

		for (auto ro : draw_nodes_) {
			ZoneScopedNC("draw_nodes_", 0xF9FFD1);
			gpu_shadow_push_constants sc = shadow_constants(ro, pass_number);
			m_shaders.shader_constants = &sc;
			m_shaders.shader_constants_size = sizeof(sc);
			if (VkResult result = m_shaders.push(mv_cmd); result != VK_SUCCESS) return rh::result::error;
			vkCmdBindIndexBuffer(mv_cmd, ro.index_buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(mv_cmd, ro.index_count, 1, ro.first_index, 0, 0);
		}

		if (pass_number == 2) {
			vkCmdEndDebugUtilsLabelEXT(mv_cmd);
			if (depth_bias_enabled) vkCmdSetDepthBiasEnable(mv_cmd, depth_bias_enabled);
		}
	}
	return rh::result::ok;
};

