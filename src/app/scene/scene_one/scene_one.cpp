#include "scene_one.h"
#include "imgui.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <numbers>
#include "../../utils/utils.h"
#include "../../loader/loader.h"


scene_one::scene_one()
{
}

rh::result scene_one::deinit(rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	const VmaAllocator allocator = ctx.rhi.allocator;
	const auto buffer = ctx.rhi.data.value();
	{
		vkDeviceWaitIdle(device);
	}
	{
		scene_graph_->deinit(ctx);
	}
	{
		skybox_->deinit(ctx);
	}
	if (ctx.rhi.descriptor_sets.value()->reset(device) != VK_SUCCESS) return rh::result::error;

	return rh::result::ok;
}

rh::result scene_one::build(const rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	const auto data = ctx.rhi.data.value();
	{
		mesh_scene mesh_graph{};
		// ReSharper disable once StringLiteralTypo
		if (const auto res = data->load_gltf_meshes(ctx, "assets\\sphere.glb", mesh_graph); res != rh::result::ok)
		{
			return res;
		}
		mesh_graph.init(ctx);
		mesh_graph.init_shadows(ctx);
		scene_graph_ = std::make_shared<mesh_scene>(std::move(mesh_graph));
		constexpr float label[4] = { 0.5f, 0.1f, 0.1f, 1.0f };
		// ReSharper disable once StringLiteralTypo
		scene_graph_->name = "deccer's cubes";
		std::ranges::copy(label, std::begin(scene_graph_->color));
	}
	{
		mesh_scene mesh_graph{};
		if (const auto res = data->load_gltf_meshes(ctx, "assets\\skybox_blue_desert.glb", mesh_graph); res != rh::result::ok)
		{
			return res;
		}
		mesh_graph.frag_path = "out/skybox.spv";
		mesh_graph.init(ctx);
		skybox_ = std::make_shared<mesh_scene>(std::move(mesh_graph));
		constexpr float label[4] = { 0.5f, 0.1f, 1.f, 1.0f };
		skybox_->name = "anime skybox";
		std::ranges::copy(label, std::begin(skybox_->color));
	}
	return rh::result::ok;
}



rh::result scene_one::depth(const rh::ctx ctx, int pass_number)
{
	VkDevice device = ctx.rhi.device;
	if (!ctx.rhi.data.has_value()) return rh::result::error;
	if (!ctx.rhi.shadow_pass_command_buffer.has_value()) return rh::result::error;
	const VkCommandBuffer mv_cmd = ctx.rhi.shadow_pass_command_buffer.value();
	const VkExtent2D frame_extent = ctx.rhi.shadow_map_extent;

	if (pass_number == 0) update_scene(ctx);

	// Scene
	{
		mesh_ctx m_ctx{};
		m_ctx.ctx = &ctx;
		m_ctx.wire_frame = toggle_wire_frame_;
		m_ctx.shadow_pass_cmd = mv_cmd;
		m_ctx.extent = frame_extent;
		if (const auto res = scene_graph_->generate_shadows(m_ctx, pass_number); res != rh::result::ok) return res;
	}

	return rh::result::ok;
}

rh::result scene_one::draw(const rh::ctx ctx)
{
	VkDevice device = ctx.rhi.device;
	if (!ctx.rhi.data.has_value()) return rh::result::error;
	if (!ctx.rhi.render_command_buffer.has_value()) return rh::result::error;
	{
		const VkCommandBuffer render_cmd = ctx.rhi.render_command_buffer.value();
		const VkExtent2D frame_extent = ctx.rhi.frame_extent;
		// Anime skybox
		{
			vkCmdSetFrontFaceEXT(render_cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
			vkCmdSetDepthTestEnableEXT(render_cmd, false);
			auto m = glm::mat4(1.0f);
			m = translate(m, glm::vec3(-scene_graph_->mesh_cam->position[0], scene_graph_->mesh_cam->position[1], scene_graph_->mesh_cam->position[2]));
			mesh_ctx m_ctx{};
			m_ctx.ctx = &ctx;
			m_ctx.wire_frame = toggle_wire_frame_;
			m_ctx.depth_enabled = false;
			m_ctx.render_cmd = render_cmd;
			m_ctx.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;;
			m_ctx.extent = frame_extent;
			m_ctx.world_transform = m;
			m_ctx.view_proj = scene_data_.view_projection;
			if (const auto res = skybox_->draw(m_ctx); res != rh::result::ok) return res;
			vkCmdSetFrontFaceEXT(render_cmd, VK_FRONT_FACE_CLOCKWISE);
		}
		// Scene
		{
			mesh_ctx m_ctx{};
			m_ctx.ctx = &ctx;
			m_ctx.wire_frame = toggle_wire_frame_;
			m_ctx.render_cmd = render_cmd;
			m_ctx.extent = frame_extent;
			m_ctx.view_proj = scene_data_.view_projection;
			if (const auto res = scene_graph_->draw(m_ctx); res != rh::result::ok) return res;
		}
	}

	return rh::result::ok;
}

rh::result scene_one::draw_ui(const rh::ctx& ctx) {
	ImGui::Begin("Scene Graph");
	{
		ImGui::Text("Camera position: (%f, %f, %f)", scene_graph_->mesh_cam->position.x, scene_graph_->mesh_cam->position.y, scene_graph_->mesh_cam->position.z);
		ImGui::Text("Camera orientation: (%f, %f)", scene_graph_->mesh_cam->pitch, scene_graph_->mesh_cam->yaw);
		ImGui::SliderFloat3("Rotate", value_ptr(scene_rot_), 0, glm::pi<float>() * 2.0f);
		ImGui::SliderFloat3("Translate", value_ptr(scene_pos_), -100.0f, 100.0f);
		ImGui::SliderFloat("Scale", &scene_scale_, 0.01f, 100.0f);
		ImGui::SliderFloat3("Sunlight direction", value_ptr(sunlight_direction_), -1.0f, 1.0f);
		sunlight_direction_ = glm::normalize(sunlight_direction_);
		ImGui::Checkbox("Wireframe", &toggle_wire_frame_);
		ImGui::Checkbox("Light view", &light_view_);
		ImGui::SliderFloat("Near plane", &near_plane_, 1.0f, 50.0f);
		ImGui::SliderFloat("distance from light camera", &distance_from_camera_, 0.f, 1.0f);
		ImGui::Text("Blending");
		ImGui::RadioButton("disabled", &blend_mode_, 0); ImGui::SameLine();
		ImGui::RadioButton("additive", &blend_mode_, 1); ImGui::SameLine();
		ImGui::RadioButton("alpha blend", &blend_mode_, 2);
	}
	ImGui::End();
	return rh::result::ok;
}

rh::result scene_one::update(const rh::ctx& ctx)
{
	scene_graph_->mesh_cam->process_sdl_event(ctx);
	return rh::result::ok;
}

std::vector<glm::vec4> scene_one::shadow_map_frustum(const glm::mat4& proj, const glm::mat4& view)
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
				const glm::vec4 point = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
				//rosy_utils::debug_print_a("\tcorner: (%f, %f, %f)\n", point.x, point.y, point.z);
				corners.push_back(point / point.w);
			}
		}
	}

	return corners;
}


void scene_one::update_scene(const rh::ctx& ctx)
{
	scene_graph_->mesh_cam->process_sdl_event(ctx);
	scene_graph_->mesh_cam->update(ctx);

	{
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
		proj[1][1] = -h;

		proj[2][2] = a;
		proj[3][2] = b;
		proj[2][3] = 1.0f;

		const auto s = (static_cast<float>(width) / static_cast<float>(height));
		const float g = 0.1f;
		auto shadow_proj = glm::mat4(1.f);
		assert(s > 0);
		const glm::mat4 view = scene_graph_->mesh_cam->get_view_matrix();

		shadow_proj = glm::perspective(fov, s, g, -500.f / 100);
		auto [sm_view_near, sm_projection_near] = scene_graph_->shadow_map_projection(sunlight_direction_, shadow_proj, view);

		shadow_proj = glm::perspective(fov, s, g, -500.f / 100);
		auto [sm_view_middle, sm_projection_middle] = scene_graph_->shadow_map_projection(sunlight_direction_, shadow_proj, view);

		shadow_proj = glm::perspective(fov, s, g, -500.f / 100);
		auto [sm_view_far, sm_projection_far] = scene_graph_->shadow_map_projection(sunlight_direction_, shadow_proj, view);

		glm::mat4 p = proj * view;
		if (light_view_)
		{
			p = sm_projection_near * sm_view_near;;
		}
		shadow_map_view_ = sm_view_near;

		scene_data_.view = view;
		scene_data_.proj = proj;
		scene_data_.view_projection = p;

		scene_data_.shadow_projection_near = sm_projection_near * sm_view_near;
		scene_data_.shadow_projection_middle = sm_projection_middle * sm_view_middle;
		scene_data_.shadow_projection_far = sm_projection_far * sm_view_far;

		scene_data_.camera_position = glm::vec4(scene_graph_->mesh_cam->position, 1.f);
		scene_data_.ambient_color = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
		scene_data_.sunlight = glm::mat4(
			glm::vec4(1.f),
			glm::vec4(1.f),
			glm::vec4(sunlight_direction_, 0.0),
			glm::vec4(0, 0, 0, 1)
		);
		scene_data_.sunlight_color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);

		glm::mat4 sv_cam = glm::inverse(shadow_map_view_);
		glm::vec4 sv_x = sv_cam[0];
		glm::vec4 sv_y = sv_cam[1];
		glm::vec4 sv_z = sv_cam[2];
		glm::vec4 sv_c = sv_cam[3];

		const float sv_u = -distance_from_camera_;

		glm::vec4 q0 = sv_c + (((sv_u * s) / g) * sv_x) + ((sv_u / g) * sv_y) + (sv_u * sv_z);
		glm::vec4 q1 = sv_c + (((sv_u * s) / g) * sv_x) - ((sv_u / g) * sv_y) + (sv_u * sv_z);
		glm::vec4 q2 = sv_c - (((sv_u * s) / g) * sv_x) - ((sv_u / g) * sv_y) + (sv_u * sv_z);
		glm::vec4 q3 = sv_c - (((sv_u * s) / g) * sv_x) + ((sv_u / g) * sv_y) + (sv_u * sv_z);

		scene_graph_->debug->set_shadow_frustum(q0, q1, q2, q3);
		scene_graph_->debug->shadow_frustum = glm::mat4(1.f);
	}
	{
		auto m = glm::mat4(1.0f);
		m = translate(m, glm::vec3(-scene_graph_->mesh_cam->position[0], scene_graph_->mesh_cam->position[1], scene_graph_->mesh_cam->position[2]));
		mesh_ctx m_ctx{};
		m_ctx.ctx = &ctx;;
		m_ctx.world_transform = m;
		skybox_->update(m_ctx, scene_data_);
	}
	{
		auto m = translate(glm::mat4(1.f), scene_pos_);
		m = rotate(m, scene_rot_[0], glm::vec3(1, 0, 0));
		m = rotate(m, scene_rot_[1], glm::vec3(0, 1, 0));
		m = rotate(m, scene_rot_[2], glm::vec3(0, 0, 1));
		m = scale(m, glm::vec3(scene_scale_, scene_scale_, scene_scale_));
		mesh_ctx m_ctx{};
		m_ctx.ctx = &ctx;
		m_ctx.world_transform = m;
		scene_graph_->update(m_ctx, scene_data_);
	}
}

