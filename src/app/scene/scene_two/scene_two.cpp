#include "scene_two.h"
#include "imgui.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <numbers>
#include "../../utils/utils.h"
#include "../../loader/loader.h"


scene_two::scene_two() 
{
}

rh::result scene_two::deinit(rh::ctx& ctx)
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

rh::result scene_two::build(const rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	const auto data = ctx.rhi.data.value();
	{
		mesh_scene mesh_graph{};
		// ReSharper disable once StringLiteralTypo
		if (const auto res = data->load_gltf_meshes(ctx, "assets\\SM_Deccer_Cubes_Textured_Complex.gltf", mesh_graph); res != rh::result::ok)
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



rh::result scene_two::depth(const rh::ctx ctx, int pass_number)
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

rh::result scene_two::draw(const rh::ctx ctx)
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
			m_ctx.view_proj = scene_graph_->scene_data.view_projection;
			if (const auto res = skybox_->draw(m_ctx); res != rh::result::ok) return res;
			vkCmdSetDepthTestEnableEXT(render_cmd, true);
			vkCmdSetFrontFaceEXT(render_cmd, VK_FRONT_FACE_CLOCKWISE);
		}
		// Scene
		{
			mesh_ctx m_ctx{};
			m_ctx.ctx = &ctx;
			m_ctx.wire_frame = toggle_wire_frame_;
			m_ctx.render_cmd = render_cmd;
			m_ctx.extent = frame_extent;
			m_ctx.view_proj = scene_graph_->scene_data.view_projection;
			if (const auto res = scene_graph_->draw(m_ctx); res != rh::result::ok) return res;
		}
	}

	return rh::result::ok;
}

rh::result scene_two::draw_ui(const rh::ctx& ctx) {
	ImGui::Begin("Scene Graph");
	{
		ImGui::Text("Camera position: (%.3f, %.3f, %.3f)", scene_graph_->mesh_cam->position.x, scene_graph_->mesh_cam->position.y, scene_graph_->mesh_cam->position.z);
		ImGui::Text("Camera orientation: (%.3f, %.3f)", scene_graph_->mesh_cam->pitch, scene_graph_->mesh_cam->yaw);
		ImGui::SliderFloat3("Rotate", value_ptr(scene_rot_), 0, glm::pi<float>() * 2.0f);
		ImGui::SliderFloat3("Translate", value_ptr(scene_pos_), -50.0f, 50.0f);
		ImGui::SliderFloat("Scale", &scene_scale_, 0.01f, 5.0f);
		ImGui::Checkbox("Wireframe", &toggle_wire_frame_);

		ImGui::Text("Light View");
		static int elem = static_cast<int>(current_view_);
		const char* elems_names[3] = { "Camera", "CSM", "Light" };
		const char* elem_name = (elem >= 0 && elem < 3) ? elems_names[elem] : "Unknown";
		current_view_ = static_cast<camera_view>(elem);
		ImGui::SliderInt("slider enum", &elem, 0, 3 - 1, elem_name); // Use ImGuiSliderFlags_NoInput flag to disable CTRL+Click here.
		ImGui::RadioButton("near", &near_plane_, 0); ImGui::SameLine();
		ImGui::RadioButton("middle", &near_plane_, 1); ImGui::SameLine();
		ImGui::RadioButton("far", &near_plane_, 2);
		ImGui::SliderFloat("distance from light camera", &distance_from_camera_, 0.f, 1.0f);
		ImGui::Text("Blending");
		ImGui::RadioButton("disabled", &blend_mode_, 0); ImGui::SameLine();
		ImGui::RadioButton("additive", &blend_mode_, 1); ImGui::SameLine();
		ImGui::RadioButton("alpha blend", &blend_mode_, 2);
	}
	ImGui::End();
	scene_graph_->draw_ui(ctx);
	return rh::result::ok;
}

rh::result scene_two::update(const rh::ctx& ctx)
{
	scene_graph_->mesh_cam->process_sdl_event(ctx);
	return rh::result::ok;
}


void scene_two::update_scene(const rh::ctx& ctx)
{
	//TODO: (skybox-fix) remove these...
	scene_graph_->near_plane = near_plane_;
	scene_graph_->scene_rot = scene_rot_;
	scene_graph_->scene_pos = scene_pos_;
	scene_graph_->scene_scale = scene_scale_;
	scene_graph_->shadow_map_view_old = shadow_map_view_;
	scene_graph_->toggle_wire_frame = toggle_wire_frame_;
	scene_graph_->current_view = current_view_;
	scene_graph_->blend_mode = blend_mode_;
	scene_graph_->distance_from_camera = distance_from_camera_;

	gpu_scene_data scene_data = scene_graph_->scene_update(ctx);

	{
		auto m = glm::mat4(1.0f);
		m = translate(m, glm::vec3(-scene_graph_->mesh_cam->position[0], scene_graph_->mesh_cam->position[1], scene_graph_->mesh_cam->position[2]));
		mesh_ctx m_ctx{};
		m_ctx.ctx = &ctx;;
		m_ctx.world_transform = m;
		skybox_->update(m_ctx, scene_data);
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
		scene_graph_->update(m_ctx, scene_data);
	}
}

