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
		shader_pipeline sp = {};
		sp.name = std::format("shadows {}", name);
		sp.with_shaders(scene_vertex_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return;
		shadow_shaders = sp;
		debug->set_sunlight(sunlight(ctx));
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

glm::mat4 mesh_scene::sunlight(const rh::ctx& ctx)
{
	if (mesh_cam == nullptr) return glm::mat4(1.f);
	glm::vec3 sl = mesh_cam->position + glm::vec3(mesh_cam->get_rotation_matrix() * glm::vec4(0.f, 0, 0, 1.f));
	//sl[1] = mesh_cam->position[1] + 10;
	sl = sl + glm::vec3(mesh_cam->get_rotation_matrix() * glm::vec4(0.f, 30.f, 1.f, 1.f));
	//sl = sl + mesh_cam->position;
	light_pos_ = sl;
	return translate(glm::mat4(1.f), glm::vec3(sl)) * light_transform_;
}

glm::mat4 mesh_scene::csm_pos(const rh::ctx& ctx)
{
	if (mesh_cam == nullptr) return glm::mat4(1.f);

	constexpr auto ndc = glm::mat4(
		glm::vec4(1.f, 0.f, 0.f, 0.f),
		glm::vec4(0.f, -1.f, 0.f, 0.f),
		glm::vec4(0.f, 0.f, 1.f, 0.f),
		glm::vec4(0.f, 0.f, 0.f, 1.f)
	);
	const auto [width, height] = ctx.rhi.frame_extent;
	glm::mat4 proj(0.0f);
	constexpr float z_near = 0.1f;
	constexpr float z_far = 1000.0f;
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	constexpr float fov = glm::radians(70.0f);
	const float h = 1.0 / tan(fov * 0.5);
	const float w = h / aspect;
	constexpr float a = -z_near / (z_far - z_near);
	constexpr float b = (z_near * z_far) / (z_far - z_near);


	proj[0][0] = w;
	proj[1][1] = h;

	proj[2][2] = a;
	proj[3][2] = b;
	proj[2][3] = 1.0f;

	proj = ndc * proj;

	if (mesh_cam == nullptr) {
		return glm::mat4(1.f);
	}
	constexpr float sv_a = 0.f;
	const float sv_b = cascade_factor_;

	const auto s = (static_cast<float>(width) / static_cast<float>(height));
	const float g = 0.1f;

	const glm::mat4 v_cam = mesh_cam->get_view_matrix();
	const glm::mat4 l_cam = sunlight(ctx);
	const glm::mat4 L = glm::inverse(l_cam);
	glm::vec4 l_x = L[0];
	glm::vec4 l_y = L[1];
	glm::vec4 l_z = L[2];
	glm::vec4 l_c = L[3];

	q0_ = l_c + (((sv_a * s) / g) * l_x) + ((sv_a / g) * l_y) + (sv_a * l_z);
	q1_ = l_c + (((sv_a * s) / g) * l_x) - ((sv_a / g) * l_y) + (sv_a * l_z);
	q2_ = l_c - (((sv_a * s) / g) * l_x) - ((sv_a / g) * l_y) + (sv_a * l_z);
	q3_ = l_c - (((sv_a * s) / g) * l_x) + ((sv_a / g) * l_y) + (sv_a * l_z);

	q4_ = l_c + (((sv_b * s) / g) * l_x) + ((sv_b / g) * l_y) + (sv_b * l_z);
	q5_ = l_c + (((sv_b * s) / g) * l_x) - ((sv_b / g) * l_y) + (sv_b * l_z);
	q6_ = l_c - (((sv_b * s) / g) * l_x) - ((sv_b / g) * l_y) + (sv_b * l_z);
	q7_ = l_c - (((sv_b * s) / g) * l_x) + ((sv_b / g) * l_y) + (sv_b * l_z);

	std::vector shadow_frustum = { q0_, q1_, q2_, q3_, q4_, q5_, q6_, q7_ };

	min_x_ = std::numeric_limits<float>::max();
	max_x_ = std::numeric_limits<float>::lowest();
	min_y_ = std::numeric_limits<float>::max();
	max_y_ = std::numeric_limits<float>::lowest();
	min_z_ = std::numeric_limits<float>::max();
	max_z_ = std::numeric_limits<float>::lowest();

	for (const auto& point : shadow_frustum)
	{

		min_x_ = std::min(min_x_, point.x);
		max_x_ = std::max(max_x_, point.x);
		min_y_ = std::min(min_y_, point.y);
		max_y_ = std::max(max_y_, point.y);
		min_z_ = std::min(min_z_, point.z);
		max_z_ = std::max(max_z_, point.z);
	}
	return translate(glm::mat4(1.f), glm::vec3(1.f));
}

void mesh_scene::draw_ui(const rh::ctx& ctx)
{
	const glm::mat4 L = sunlight(ctx);
	ImGui::Begin("Sunlight & Shadow");
	{
		bool rotate = false;
		ImGui::Text("Light position: (%.3f, %.3f, %.3f)", light_pos_.x, light_pos_.y, light_pos_.z);
		ImGui::Text("Light direction: (%.3f, %.3f, %.3f)", L[2][0], L[2][1], L[2][2]);

		ImGui::Text("q0_: (%.3f, %.3f, %.3f)", q0_.x, q0_.y, q0_.z);
		ImGui::Text("q1_: (%.3f, %.3f, %.3f)", q1_.x, q1_.y, q1_.z);
		ImGui::Text("q2_: (%.3f, %.3f, %.3f)", q2_.x, q2_.y, q2_.z);
		ImGui::Text("q3_: (%.3f, %.3f, %.3f)", q3_.x, q3_.y, q3_.z);

		ImGui::Text("q4_: (%.3f, %.3f, %.3f)", q4_.x, q4_.y, q4_.z);
		ImGui::Text("q5_: (%.3f, %.3f, %.3f)", q5_.x, q5_.y, q5_.z);
		ImGui::Text("q6_: (%.3f, %.3f, %.3f)", q6_.x, q6_.y, q6_.z);
		ImGui::Text("q7_: (%.3f, %.3f, %.3f)", q7_.x, q7_.y, q7_.z);

		ImGui::Text("min: (%.3f, %.3f, %.3f)", min_x_, min_y_, min_z_);
		ImGui::Text("max: (%.3f, %.3f, %.3f)", max_x_, max_y_, max_z_);
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
		ImGui::SliderFloat("Cascade factor", &cascade_factor_, 0, 10, "%.3f");
		ImGui::Text("Depth bias");
		ImGui::Checkbox("Enabled", &depth_bias_enabled);
		ImGui::SliderFloat("constant", &depth_bias_constant, 0.f, 1000.0f);
		ImGui::SliderFloat("clamp", &depth_bias_clamp, 0.f, 1000.0f);
		ImGui::SliderFloat("slope factor", &depth_bias_slope_factor, 0.f, 1000.0f);
	}
	ImGui::End();
};

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
		debug->set_sunlight(sunlight(*ctx.ctx));
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
		if (depth_bias_enabled) {
			vkCmdSetDepthBiasEnable(mv_cmd, depth_bias_enabled);
			VkDepthBiasInfoEXT db_info{};
			//db_info.sType = VK_STRUCTURE_TYPE_DEPTH_BIAS_INFO_EXT;
			//db_info.depthBiasConstantFactor = depth_bias_constant;
			//db_info.depthBiasClamp = depth_bias_clamp;
			//db_info.depthBiasSlopeFactor = depth_bias_slope_factor;
			//vkCmdSetDepthBias2EXT(mv_cmd, &db_info);
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
	return rh::result::ok;
};


std::vector<glm::vec4> mesh_scene::shadow_map_frustum(const glm::mat4& proj, const glm::mat4& view)
{
	constexpr auto fix = glm::mat4(
		glm::vec4(1.f, 0.f, 0.f, 0.f),
		glm::vec4(0.f, 1.f, 0.f, 0.f),
		glm::vec4(0.f, 0.f, 1.f, 0.f),
		glm::vec4(0.f, 0.f, 0.f, 1.f)
	);
	const auto inv = inverse(proj * view * fix);

	std::vector<glm::vec4> corners;
	for (unsigned int x = 0; x < 2; ++x)
	{
		for (unsigned int y = 0; y < 2; ++y)
		{
			for (unsigned int z = 0; z < 2; ++z)
			{
				const glm::vec4 point = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, z, 1.0f);
				//rosy_utils::debug_print_a("\tcorner: (%f, %f, %f)\n", point.x, point.y, point.z);
				corners.push_back(point / point.w);
			}
		}
	}

	return corners;
}

glm::mat4 mesh_scene::shadow_map_view(const std::vector<glm::vec4>& shadow_frustum, const glm::vec3 light_direction)
{
	auto center = glm::vec3(0, 0, 0);
	for (const auto& v : shadow_frustum) center += glm::vec3(v);
	center /= shadow_frustum.size();
	return lookAt(center + light_direction, center, glm::vec3(0.0f, 1.0f, 0.0f));
}


shadow_map mesh_scene::shadow_map_projection(const std::vector<glm::vec4>& shadow_frustum, const glm::mat4& shadow_map_view)
{
	float min_x = std::numeric_limits<float>::max();
	float max_x = std::numeric_limits<float>::lowest();
	float min_y = std::numeric_limits<float>::max();
	float max_y = std::numeric_limits<float>::lowest();
	float min_z = std::numeric_limits<float>::max();
	float max_z = std::numeric_limits<float>::lowest();

	for (const auto& v : shadow_frustum)
	{
		//rosy_utils::debug_print_a("\tcorner: (%f, %f, %f)\n", v.x, v.y, v.z);
		//rosy_utils::debug_print_a("\t bef point.z: %f min_z: %f max_z: %f\n\n", v.z, min_z, max_z);
		const auto point = shadow_map_view * v;
		min_x = std::min(min_x, point.x);
		max_x = std::max(max_x, point.x);
		min_y = std::min(min_y, point.y);
		max_y = std::max(max_y, point.y);
		min_z = std::min(min_z, point.z);
		max_z = std::max(max_z, point.z);
		//rosy_utils::debug_print_a("\t after point.z: %f min_z: %f max_z: %f\n\n", point.z,  min_z, max_z);
	}

	constexpr float z_offset = 10.0f;
	if (min_z < 0) min_z *= z_offset;
	else min_z /= z_offset;
	if (max_z < 0) max_z /= z_offset;
	else max_z *= z_offset;


	//rosy_utils::debug_print_a("min_x: %f max_x: %f min_y: %f, max_y: %f min_z: %f max_z: %f\n\n", min_x, max_x, min_y, max_y, min_z, max_z);


	const glm::mat4 p = glm::ortho(
		max_x, min_x,
		max_y, min_y,
		-1 * min_z, -1 * max_z);



	shadow_map map{};
	map.view = shadow_map_view;
	map.projection = p;

	return map;
}

shadow_map mesh_scene::shadow_map_projection(const rh::ctx& ctx, const glm::vec3 light_direction, const glm::mat4& p, const glm::mat4& world_view)
{
	const glm::mat4 m_sk = csm_pos(ctx);
	const auto frustum = shadow_map_frustum(p, world_view);
	const auto shadow_view = shadow_map_view(frustum, glm::normalize(light_direction));
	return shadow_map_projection(frustum, shadow_view);
}