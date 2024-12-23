#include "scene_two.h"
#include "imgui.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <numbers>
#include "../../utils/utils.h"
#include "../../loader/loader.h"


scene_two::scene_two() :  // NOLINT(modernize-use-equals-default)
	camera_(glm::vec3{ 10.2f, 0.6f, -13.3f })
{
	camera_.pitch = 0.36f;
	camera_.yaw = 1.f;
}

rh::result scene_two::build(const rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	const auto data = ctx.rhi.data.value();
	descriptor_allocator_growable descriptor_allocator = ctx.rhi.descriptor_allocator.value();
	{
		std::vector<VkDescriptorSetLayout> layouts;
		descriptor_layout_builder layout_builder;
		layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return rh::result::error;
		gpu_scene_data_descriptor_layout_ = set;
		layouts.push_back(set);
	}
	{
		mesh_scene mesh_graph{};
		mesh_graph.init(ctx);
		mesh_graph.init_shadows(ctx);
		// ReSharper disable once StringLiteralTypo
		if (const auto res = data->load_gltf_meshes(ctx, "assets\\SM_Deccer_Cubes_Textured_Complex.gltf", mesh_graph); res != rh::result::ok)
		{
			return res;
		}
		scene_graph_ = std::make_shared<mesh_scene>(std::move(mesh_graph));
		constexpr float label[4] = { 0.5f, 0.1f, 0.1f, 1.0f };
		// ReSharper disable once StringLiteralTypo
		scene_graph_->name = "deccer's cubes";
		std::ranges::copy(label, std::begin(scene_graph_->color));
	}
	{
		mesh_scene mesh_graph{};
		mesh_graph.frag_path = "out/skybox.spv";
		mesh_graph.init(ctx);
		if (const auto res = data->load_gltf_meshes(ctx, "assets\\skybox_blue_desert.glb", mesh_graph); res != rh::result::ok)
		{
			return res;
		}
		skybox_ = std::make_shared<mesh_scene>(std::move(mesh_graph));
		constexpr float label[4] = { 0.5f, 0.1f, 1.f, 1.0f };
		skybox_->name = "anime skybox";
		std::ranges::copy(label, std::begin(skybox_->color));
	}
	return rh::result::ok;
}



rh::result scene_two::depth(rh::ctx ctx)
{
	VkDevice device = ctx.rhi.device;
	if (!ctx.rhi.data.has_value()) return rh::result::error;
	if (!ctx.rhi.frame_data.has_value()) return rh::result::error;
	auto [opt_command_buffers, opt_image_available_semaphores, opt_render_finished_semaphores, opt_in_flight_fence,
		opt_command_pool, opt_frame_descriptors, opt_gpu_scene_buffer] = ctx.rhi.frame_data.value();
	{
		if (!opt_command_buffers.has_value()) return rh::result::error;
		if (!opt_frame_descriptors.has_value()) return rh::result::error;
	}
	VkCommandBuffer cmd = opt_command_buffers.value();
	descriptor_allocator_growable frame_descriptors = opt_frame_descriptors.value();
	allocated_buffer gpu_scene_buffer = opt_gpu_scene_buffer.value();
	VkExtent2D frame_extent = ctx.rhi.shadow_map_extent;

	update_scene(ctx, gpu_scene_buffer);

	// Set descriptor sets
	auto [desc_result, global_descriptor] = frame_descriptors.allocate(device, gpu_scene_data_descriptor_layout_.value());
	if (desc_result != VK_SUCCESS) return rh::result::error;
	{
		// Global descriptor
		{

			descriptor_writer writer;
			writer.write_buffer(0, gpu_scene_buffer.buffer, sizeof(gpu_scene_data), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			writer.update_set(device, global_descriptor);
		}
	}
	// Scene
	{
		mesh_ctx m_ctx{};
		m_ctx.ctx = &ctx;
		m_ctx.wire_frame = toggle_wire_frame_;
		m_ctx.cmd = cmd;
		m_ctx.extent = frame_extent;
		m_ctx.global_descriptor = &global_descriptor;
		if (const auto res = scene_graph_->generate_shadows(m_ctx); res != rh::result::ok) return res;
	}

	return rh::result::ok;
}

rh::result scene_two::draw(rh::ctx ctx)
{
	VkDevice device = ctx.rhi.device;
	if (!ctx.rhi.data.has_value()) return rh::result::error;
	if (!ctx.rhi.frame_data.has_value()) return rh::result::error;
	auto [opt_command_buffers, opt_image_available_semaphores, opt_render_finished_semaphores, opt_in_flight_fence,
		opt_command_pool, opt_frame_descriptors, opt_gpu_scene_buffer] = ctx.rhi.frame_data.value();
	{
		if (!opt_command_buffers.has_value()) return rh::result::error;
		if (!opt_frame_descriptors.has_value()) return rh::result::error;
	}
	VkCommandBuffer cmd = opt_command_buffers.value();
	descriptor_allocator_growable frame_descriptors = opt_frame_descriptors.value();
	allocated_buffer gpu_scene_buffer = opt_gpu_scene_buffer.value();
	VkExtent2D frame_extent = ctx.rhi.frame_extent;

	// Set descriptor sets
	auto [desc_result, global_descriptor] = frame_descriptors.allocate(device, gpu_scene_data_descriptor_layout_.value());
	if (desc_result != VK_SUCCESS) return rh::result::error;
	{
		// Global descriptor
		{

			descriptor_writer writer;
			writer.write_buffer(0, gpu_scene_buffer.buffer, sizeof(gpu_scene_data), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			writer.update_set(device, global_descriptor);
		}
	}
	{
		// Anime skybox
		{
			auto m = glm::mat4(1.0f);
			m = translate(m, glm::vec3(-camera_.position[0], camera_.position[1], camera_.position[2]));
			mesh_ctx m_ctx{};
			m_ctx.ctx = &ctx;
			m_ctx.wire_frame = toggle_wire_frame_;
			m_ctx.depth_enabled = false;
			m_ctx.cmd = cmd;
			m_ctx.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;;
			m_ctx.extent = frame_extent;
			m_ctx.global_descriptor = &global_descriptor;
			m_ctx.world_transform = m;
			if (const auto res = skybox_->draw(m_ctx); res != rh::result::ok) return res;
		}
		// Scene
		{
			mesh_ctx m_ctx{};
			m_ctx.ctx = &ctx;
			m_ctx.wire_frame = toggle_wire_frame_;
			m_ctx.cmd = cmd;
			m_ctx.extent = frame_extent;
			m_ctx.global_descriptor = &global_descriptor;
			if (const auto res = scene_graph_->draw(m_ctx); res != rh::result::ok) return res;
		}
	}

	return rh::result::ok;
}

rh::result scene_two::draw_ui(const rh::ctx& ctx) {
	ImGui::Begin("Scene Graph");
	{
		ImGui::Text("Camera position: (%f, %f, %f)", camera_.position.x, camera_.position.y, camera_.position.z);
		ImGui::Text("Camera orientation: (%f, %f)", camera_.pitch, camera_.yaw);
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

rh::result scene_two::deinit(rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	const VmaAllocator allocator = ctx.rhi.allocator;
	const auto buffer = ctx.rhi.data.value();
	{
		vkDeviceWaitIdle(device);
	}
	if (ctx.rhi.descriptor_allocator.has_value()) ctx.rhi.descriptor_allocator.value().clear_pools(device);
	{
		scene_graph_->deinit(ctx);
	}
	{
		skybox_->deinit(ctx);
	}
	if (gpu_scene_data_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device, gpu_scene_data_descriptor_layout_.value(), nullptr);
	}

	return rh::result::ok;
}

rh::result scene_two::update(const rh::ctx& ctx)
{
	camera_.process_sdl_event(ctx);
	return rh::result::ok;
}

std::vector<glm::vec4> scene_two::shadow_map_frustum(const glm::mat4& proj, const glm::mat4& view)
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

glm::mat4 scene_two::shadow_map_view(const std::vector<glm::vec4>& shadow_frustum, const glm::vec3 light_direction)
{
	auto center = glm::vec3(0, 0, 0);
	for (const auto& v : shadow_frustum) center += glm::vec3(v);
	center /= shadow_frustum.size();
	return lookAt(center + light_direction, center, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 scene_two::shadow_map_projection(const std::vector<glm::vec4>& shadow_frustum, const glm::mat4& shadow_map_view)
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

	auto m = shadow_map_view;
	auto extent = glm::mat4(1.f);
	extent[0][0] = min_x;
	extent[1][1] = min_y;
	extent[2][2] = min_z;
	extent = glm::translate(extent, glm::vec3(max_x, max_y, max_z));

	//rosy_utils::debug_print_a("min_x: %f max_x: %f min_y: %f, max_y: %f min_z: %f max_z: %f\n\n", min_x, max_x, min_y, max_y, min_z, max_z);

	shadow_map_view_ = shadow_map_view;

	const glm::mat4 p = glm::ortho(
		min_x, max_x,
		  min_y,   max_y,
		  -1 * min_z,   -1 * max_z);


	const auto rv = p * shadow_map_view;

	

	return rv;
}

glm::mat4 scene_two::shadow_map_projection(const glm::vec3 light_direction, const glm::mat4& p, const glm::mat4& world_view)
{
	const auto frustum = shadow_map_frustum(p, world_view);
	const auto shadow_view = shadow_map_view(frustum, glm::normalize(light_direction));
	return shadow_map_projection(frustum, shadow_view);
}

void scene_two::update_scene(const rh::ctx& ctx, const allocated_buffer& gpu_scene_buffer)
{
	camera_.process_sdl_event(ctx);
	camera_.update(ctx);

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
		if (s > 0) {
			shadow_proj = glm::perspective(
				fov,
				s,
				g,
				-250.f / near_plane_
			);
		}
		const glm::mat4 view = camera_.get_view_matrix();
		glm::mat4 sp = shadow_map_projection(sunlight_direction_, shadow_proj, view);
		glm::mat4 p = proj * view;
		if (light_view_)
		{
			p = sp;
		}
		scene_data_.view = view;
		scene_data_.proj = proj;
		scene_data_.view_projection = p;
		scene_data_.shadow_projection = sp;
		scene_data_.camera_position = glm::vec4(camera_.position, 1.f);
		scene_data_.ambient_color = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
		scene_data_.sunlight_direction = glm::vec4(sunlight_direction_, 0.0);
		scene_data_.sunlight_color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);

		glm::mat4 sv_cam = glm::inverse(shadow_map_view_);
		//glm::mat4 sv_cam = glm::inverse(view);
		glm::vec4 sv_x = sv_cam[0];
		glm::vec4 sv_y = sv_cam[1];
		glm::vec4 sv_z = sv_cam[2];
		glm::vec4 sv_c = sv_cam[3];


		/*glm::vec4 sv_x = glm::vec4(sv_cam[0][0], sv_cam[1][0], sv_cam[2][0], sv_cam[3][0]);
		glm::vec4 sv_y = glm::vec4(sv_cam[0][1], sv_cam[1][1], sv_cam[2][1], sv_cam[3][1]);
		glm::vec4 sv_z = glm::vec4(sv_cam[0][2], sv_cam[1][2], sv_cam[2][2], sv_cam[3][2]);
		glm::vec4 sv_c = glm::vec4(sv_cam[0][3], sv_cam[1][3], sv_cam[2][3], sv_cam[3][3]);*/
		const float sv_u = -distance_from_camera_;

		glm::vec4 q0 = sv_c + (((sv_u * s) / g) * sv_x) + ((sv_u / g) * sv_y) + (sv_u * sv_z);
		glm::vec4 q1 = sv_c + (((sv_u * s) / g) * sv_x) - ((sv_u / g) * sv_y) + (sv_u * sv_z);
		glm::vec4 q2 = sv_c - (((sv_u * s) / g) * sv_x) - ((sv_u / g) * sv_y) + (sv_u * sv_z);
		glm::vec4 q3 = sv_c - (((sv_u * s) / g) * sv_x) + ((sv_u / g) * sv_y) + (sv_u * sv_z);

		/*rosy_utils::debug_print_a("sv_c: (%f, %f, %f)\n", sv_c.x, sv_c.y, sv_c.z);
		rosy_utils::debug_print_a("q0: (%f, %f, %f)\n", q0.x, q0.y, q0.z);
		rosy_utils::debug_print_a("q1: (%f, %f, %f)\n", q1.x, q1.y, q1.z);
		rosy_utils::debug_print_a("q2: (%f, %f, %f)\n", q2.x, q2.y, q2.z);
		rosy_utils::debug_print_a("q3: (%f, %f, %f)\n", q3.x, q3.y, q3.z);*/
		scene_graph_->debug->set_shadow_frustum(q0, q1, q2, q3);
		scene_graph_->debug->shadow_frustum = glm::mat4(1.f);
	}

	{
		const VmaAllocator allocator = ctx.rhi.allocator;
		// copy memory
		void* data_pointer;
		vmaMapMemory(allocator, gpu_scene_buffer.allocation, &data_pointer);
		memcpy(data_pointer, &scene_data_, sizeof(scene_data_));
		vmaUnmapMemory(allocator, gpu_scene_buffer.allocation);

	}
	{
		auto m = glm::mat4(1.0f);
		m = translate(m, glm::vec3(-camera_.position[0], camera_.position[1], camera_.position[2]));
		mesh_ctx m_ctx{};
		m_ctx.ctx = &ctx;;
		m_ctx.world_transform = m;
		skybox_->update(m_ctx);
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
		scene_graph_->update(m_ctx);
	}
}

