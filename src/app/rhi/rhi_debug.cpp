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
	{
		shader_pipeline sp = {};
		sp.name = std::format("scene {}", name);
		sp.shader_constants_size = sizeof(debug_draw_push_constants);
		sp.primitive = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		sp.with_shaders(scene_vertex_shader, scene_fragment_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return;
		shaders = sp;
	}
}

void debug_gfx::deinit(const rh::ctx& ctx) const
{
	const VkDevice device = ctx.rhi.device;
	if (shaders.has_value()) {
		shaders.value().deinit(device);
	}
}

rh::result debug_gfx::draw(mesh_ctx ctx, VkDeviceAddress scene_buffer_address)
{
	if (lines.size() == 0) return rh::result::ok;
	auto render_cmd = ctx.render_cmd;

	{
		VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label(name.c_str(), color);
		vkCmdBeginDebugUtilsLabelEXT(render_cmd, &mesh_draw_label);
	}
	vkCmdSetLineWidth(render_cmd, 5.f);

	shader_pipeline m_shaders = shaders.value();
	{
		m_shaders.viewport_extent = ctx.extent;
		m_shaders.wire_frames_enabled = ctx.wire_frame;
		m_shaders.depth_enabled = ctx.depth_enabled;
		m_shaders.front_face = ctx.front_face;
		if (VkResult result = m_shaders.shade(render_cmd); result != VK_SUCCESS) return rh::result::error;
	}

	vkCmdSetPrimitiveTopologyEXT(render_cmd, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	for (auto line : lines) {
		auto l = line;
		l.world_matrix = ctx.view_proj * l.world_matrix;
		m_shaders.shader_constants = &l;
		m_shaders.shader_constants_size = sizeof(line);
		if (VkResult result = m_shaders.push(render_cmd); result != VK_SUCCESS) return rh::result::error;
		vkCmdDraw(render_cmd, 2, 1, 0, 0);
	}
	if (shadow_frustum.has_value()) {
		for (auto line : shadow_box_lines) {
			auto shadow_line = line;
			//shadow_line.world_matrix = ctx.view_proj * shadow_frustum.value();
			shadow_line.world_matrix = ctx.view_proj * line.world_matrix;
			m_shaders.shader_constants = &shadow_line;
			m_shaders.shader_constants_size = sizeof(shadow_line);
			if (VkResult result = m_shaders.push(render_cmd); result != VK_SUCCESS) return rh::result::error;
			vkCmdDraw(render_cmd, 2, 1, 0, 0);
		}
	}
	for (auto line : sunlight_lines) {
		auto shadow_line = line;
		shadow_line.world_matrix = ctx.view_proj * line.world_matrix;
		m_shaders.shader_constants = &shadow_line;
		m_shaders.shader_constants_size = sizeof(shadow_line);
		if (VkResult result = m_shaders.push(render_cmd); result != VK_SUCCESS) return rh::result::error;
		vkCmdDraw(render_cmd, 2, 1, 0, 0);
	}
	vkCmdSetPrimitiveTopologyEXT(render_cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	vkCmdEndDebugUtilsLabelEXT(render_cmd);
	return rh::result::ok;
};

void debug_gfx::set_sunlight(glm::mat4 sunlight)
{
	sunlight_lines.clear();
	// Sunlight direction
	debug_draw_push_constants line{};
	constexpr auto origin = glm::vec4(0.f, 0.f, 0.f, 1.f);
	line.world_matrix = sunlight;
	line.p1 = origin;

	line.color = glm::vec4(1.0f, 0.843f, 0.0f, 1.0f);
	line.p2 = glm::vec4(1.f, 0.f, 0.f, 1.f);;
	sunlight_lines.push_back(line);
	line.p2 = glm::vec4(0.f, 1.f, 0.f, 1.f);;
	sunlight_lines.push_back(line);
	line.color = glm::vec4(1.0f, 0.843f, 0.843f, 1.0f);
	line.p2 = glm::vec4(0.f, 0.f, 1.f, 1.f);;
	sunlight_lines.push_back(line);
}

debug_frustum debug_frustum::from_bounds(const float min_x, const float max_x, const float min_y, const float max_y, const float min_z, const float max_z)
{
	debug_frustum f{};
	f.points[0] = glm::vec4{ min_x, min_y, min_z, 1.f };
	f.points[1] = glm::vec4{ max_x, min_y, min_z, 1.f };
	f.points[2] = glm::vec4{ min_x, max_y, min_z, 1.f };
	f.points[3] = glm::vec4{ max_x, max_y, min_z, 1.f };

	f.points[4] = glm::vec4{ min_x, min_y, max_z, 1.f };
	f.points[5] = glm::vec4{ max_x, min_y, max_z, 1.f };
	f.points[6] = glm::vec4{ min_x, max_y, max_z, 1.f };
	f.points[7] = glm::vec4{ max_x, max_y, max_z, 1.f };
	return f;
}


void debug_gfx::set_shadow_frustum(debug_frustum frustum)
{
	{
		shadow_box_lines.clear();
		const auto [transform, points] = frustum;
		glm::mat4 m = transform;
		debug_draw_push_constants line{};
		line.world_matrix = m;

		auto red = glm::vec4(1.f, 0.f, 0.f, 1.f);
		auto blue = glm::vec4(0.f, 0.f, 1.f, 1.f);
		auto green = glm::vec4(0.f, 1.f, 0.f, 1.f);
		auto yellow = glm::vec4(1.f, 1.f, 0.f, 1.f);

		// Shadow frustum near
		//rosy_utils::debug_print_a("\t near_line_1: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = red;
		line.p1 = points[0];
		line.p2 = points[1];
		shadow_box_lines.push_back(line);

		//rosy_utils::debug_print_a("\t near_line_2: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = red;
		line.p1 = points[1];
		line.p2 = points[3];
		shadow_box_lines.push_back(line);

		//rosy_utils::debug_print_a("\t near_line_3 %f.3\n", glm::distance2(points[3], points[7]));
		line.color = red;
		line.p1 = points[0];
		line.p2 = points[2];
		shadow_box_lines.push_back(line);

		//rosy_utils::debug_print_a("\t near_line_4: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = red;
		line.p1 = points[3];
		line.p2 = points[2];
		shadow_box_lines.push_back(line);


		// Shadow frustum far
		//rosy_utils::debug_print_a("\t far_line_1: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = blue;
		line.p1 = points[4];
		line.p2 = points[5];
		shadow_box_lines.push_back(line);

		//rosy_utils::debug_print_a("\t far_line_2: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = blue;
		line.p1 = points[5];
		line.p2 = points[7];

		//rosy_utils::debug_print_a("\t far_line_3: %f.3\n", glm::distance2(points[3], points[7]));
		shadow_box_lines.push_back(line);
		line.color = blue;
		line.p1 = points[4];
		line.p2 = points[6];
		shadow_box_lines.push_back(line);

		//rosy_utils::debug_print_a("\t far_line_4: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = blue;
		line.p1 = points[7];
		line.p2 = points[6];
		shadow_box_lines.push_back(line);

		// connect near and far sides of frustum
		//rosy_utils::debug_print_a("\t connect_line_1: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = yellow;
		line.p1 = points[0];
		line.p2 = points[4];
		shadow_box_lines.push_back(line);

		//rosy_utils::debug_print_a("\t connect_line_2: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = yellow;
		line.p1 = points[1];
		line.p2 = points[5];

		//rosy_utils::debug_print_a("\t connect_line_3: %f.3\n", glm::distance2(points[3], points[7]));
		shadow_box_lines.push_back(line);
		line.color = green;
		line.p1 = points[2];
		line.p2 = points[6];
		shadow_box_lines.push_back(line);

		//rosy_utils::debug_print_a("\t connect_line_4: %f.3\n", glm::distance2(points[3], points[7]));
		line.color = green;
		line.p1 = points[3];
		line.p2 = points[7];
		shadow_box_lines.push_back(line);
		//rosy_utils::debug_print_a("\n\n");
	}
}