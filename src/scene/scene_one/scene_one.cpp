#include "scene_one.h"

#include "imgui.h"
#include "../../utils/utils.h"
#include "../../loader/loader.h"


rh::result scene_one::build(const rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	auto data = ctx.rhi.data.value();
	descriptor_allocator global_descriptor_allocator = ctx.rhi.global_descriptor_allocator.value();
	{
		descriptor_layout_builder layout_builder;
		layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return rh::result::error;
		gpu_scene_data_descriptor_layout_ = set;
	}
	{
		descriptor_layout_builder layout_builder;
		layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return rh::result::error;
		single_image_descriptor_layout_ = set;
	}
	std::vector<char> vert_shader_code;
	std::vector<char> frag_shader_code;
	try
	{
		vert_shader_code = read_file("out/vert.spv");
		frag_shader_code = read_file("out/tex_image.frag.spv");
	}
	catch (const std::exception& e)
	{
		rosy_utils::debug_print_a("error reading shader files! %s", e.what());
		return rh::result::error;
	}
	
	shader_pipeline sp = {};
	sp.layouts = &single_image_descriptor_layout_.value();
	sp.num_layouts = 1;
	sp.name = "test";
	sp.with_shaders(vert_shader_code, frag_shader_code);
	if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return rh::result::error;
	test_mesh_pipeline_ = sp;


	// ReSharper disable once StringLiteralTypo
	if (auto load_result = data->load_gltf_meshes("assets\\basicmesh.glb"); load_result.has_value())
	{
		test_meshes_ = load_result.value();
	}
	else
	{
		return rh::result::error;
	}

	const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));

	{
		//checkerboard image
		constexpr size_t image_dimensions = static_cast<size_t>(16) * 16;
		std::array<uint32_t, image_dimensions > pixels;
		for (int x = 0; x < 16; x++) {
			for (int y = 0; y < 16; y++) {
				pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
			}
		}
		auto [result, image] = data->create_image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT, false);
		if (result != VK_SUCCESS) return rh::result::error;
		error_checkerboard_image_ = image;
	}
	{
		VkSamplerCreateInfo sample = {};
		sample.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sample.magFilter = VK_FILTER_NEAREST;
		sample.minFilter = VK_FILTER_NEAREST;
		VkSampler sampler;
		if (VkResult result = vkCreateSampler(device, &sample, nullptr, &sampler); result != VK_SUCCESS) return rh::result::error;
		default_sampler_nearest_ = sampler;
	}
	{
		auto [image_set_result, image_set] = global_descriptor_allocator.allocate(device, single_image_descriptor_layout_.value());
		if (image_set_result != VK_SUCCESS) return  rh::result::error;
		{
			descriptor_writer writer;
			writer.write_image(0, error_checkerboard_image_->image_view, default_sampler_nearest_.value(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			writer.update_set(device, image_set);
			sphere_image_descriptor_set_ = image_set;
		}
	}

	return rh::result::ok;
}

rh::result scene_one::draw(rh::ctx ctx)
{
	VkDevice device = ctx.rhi.device;
	if (!ctx.rhi.data.has_value()) return rh::result::error;
	if (!ctx.rhi.frame_data.has_value()) return rh::result::error;
	auto data = ctx.rhi.data.value();
	auto [opt_command_buffers, opt_image_available_semaphores, opt_render_finished_semaphores, opt_in_flight_fence,
		opt_command_pool, opt_frame_descriptors, opt_gpu_scene_buffer] = ctx.rhi.frame_data.value();
	{
		if (!opt_command_buffers.has_value()) return rh::result::error;
		if (!opt_frame_descriptors.has_value()) return rh::result::error;
	}
	VkCommandBuffer cmd = opt_command_buffers.value();
	descriptor_allocator_growable frame_descriptors = opt_frame_descriptors.value();
	allocated_buffer gpu_scene_buffer = opt_gpu_scene_buffer.value();
	shader_pipeline shaders = test_mesh_pipeline_.value();
	VkExtent2D frame_extent = ctx.rhi.frame_extent;
	VkDescriptorSet sphere_image_set = sphere_image_descriptor_set_.value();
	{
		// meshes
		{
	
			// bind a texture
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shaders.pipeline_layout.value(), 0, 1, &sphere_image_set, 0, nullptr);
	
			//create a descriptor set that binds that data and update it
			auto [desc_result, desc_set] = frame_descriptors.allocate(device, gpu_scene_data_descriptor_layout_.value());
			if (desc_result != VK_SUCCESS) return rh::result::error;
			VkDescriptorSet global_descriptor = desc_set;
	
			descriptor_writer writer;
			writer.write_buffer(0, gpu_scene_buffer.buffer, sizeof(gpu_scene_data), 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			writer.update_set(device, global_descriptor);
	
			float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
			VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label("meshes", color);
			vkCmdBeginDebugUtilsLabelEXT(cmd, &mesh_draw_label);
	
	
			gpu_draw_push_constants push_constants;
			auto m = glm::mat4(1.0f);
	
			auto camera_pos = glm::vec3(0.0f, 0.0f, -10.0f);
			auto camera_target = glm::vec3(0.0f, 0.0f, 0.0f);
			auto camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
	
			glm::mat4 view = lookAt(camera_pos, camera_target, camera_up);
	
			m = translate(m, glm::vec3{ model_x_, model_y_, model_z_ });
			m = rotate(m, model_rot_x_, glm::vec3(1, 0, 0));
			m = rotate(m, model_rot_y_, glm::vec3(0, 1, 0));
			m = rotate(m, model_rot_z_, glm::vec3(0, 0, 1));
			m = scale(m, glm::vec3(model_scale_, model_scale_, model_scale_));
	
			float z_near = 0.1f;
			float z_far = 1000.0f;
			float aspect = static_cast<float>(frame_extent.width) / static_cast<float>(frame_extent.height);
			constexpr float fov = glm::radians(70.0f);
			float h = 1.0 / tan(fov * 0.5);
			float w = h / aspect;
			float a = -z_near / (z_far - z_near);
			float b = (z_near * z_far) / (z_far - z_near);
	
			glm::mat4 proj(0.0f);
	
			proj[0][0] = w;
			proj[1][1] = -h;
	
			proj[2][2] = a;
			proj[3][2] = b;
			proj[2][3] = 1.0f;
			push_constants.world_matrix = proj * view * m;
	
			if (test_meshes_.size() > 0)
			{
				size_t mesh_index = 1;
				auto mesh = test_meshes_[mesh_index];
				push_constants.vertex_buffer = mesh->mesh_buffers.vertex_buffer_address;
				shaders.viewport_extent = frame_extent;
				shaders.shader_constants = &push_constants;
				shaders.shader_constants_size = sizeof(push_constants);
				shaders.wire_frames_enabled = toggle_wire_frame_;
				shaders.depth_enabled = true;
				shaders.blending = static_cast<shader_blending>(blend_mode_);
				if (VkResult result = shaders.shade(cmd); result != VK_SUCCESS) return rh::result::error;
				vkCmdBindIndexBuffer(cmd, mesh->mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, mesh->surfaces[0].count, 1, mesh->surfaces[0].start_index,
					0, 0);
			}
	
			vkCmdEndDebugUtilsLabelEXT(cmd);
		}
	}

	return rh::result::ok;
}

rh::result scene_one::draw_ui(const rh::ctx& ctx) {
	ImGui::SliderFloat("Rotate X", &model_rot_x_, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Rotate Y", &model_rot_y_, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Rotate Z", &model_rot_z_, 0, glm::pi<float>() * 2.0f);
	ImGui::SliderFloat("Translate X", &model_x_, -100.0f, 100.0f);
	ImGui::SliderFloat("Translate Y", &model_y_, -100.0f, 100.0f);
	ImGui::SliderFloat("Translate Z", &model_z_, -1000.0f, 10.0f);
	ImGui::SliderFloat("Scale", &model_scale_, 0.1f, 10.0f);
	ImGui::Checkbox("Wireframe", &toggle_wire_frame_);
	ImGui::Text("Blending");
	ImGui::RadioButton("disabled", &blend_mode_, 0); ImGui::SameLine();
	ImGui::RadioButton("additive", &blend_mode_, 1); ImGui::SameLine();
	ImGui::RadioButton("alpha blend", &blend_mode_, 2);
	return rh::result::ok;
}

rh::result scene_one::deinit(const rh::ctx& ctx) const
{
	const VkDevice device = ctx.rhi.device;
	auto buffer = ctx.rhi.data.value();
	{
		vkDeviceWaitIdle(device);
	}
	{
		if (ctx.rhi.global_descriptor_allocator.has_value())
		{
			ctx.rhi.global_descriptor_allocator.value().clear_descriptors(device);
		}
		if (default_sampler_nearest_.has_value()) vkDestroySampler(device, default_sampler_nearest_.value(), nullptr);
		if (black_image_.has_value())  buffer->destroy_image(black_image_.value());
		if (error_checkerboard_image_.has_value())  buffer->destroy_image(error_checkerboard_image_.value());
	}

	for (std::shared_ptr<mesh_asset> mesh : test_meshes_)
	{
		gpu_mesh_buffers rectangle = mesh.get()->mesh_buffers;
		buffer->destroy_buffer(rectangle.vertex_buffer);
		buffer->destroy_buffer(rectangle.index_buffer);
		mesh.reset();
	}
	if (test_mesh_pipeline_.has_value()) {
		test_mesh_pipeline_.value().deinit(device);
	}
	if (gpu_scene_data_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device, gpu_scene_data_descriptor_layout_.value(), nullptr);
	}
	if (single_image_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device, single_image_descriptor_layout_.value(), nullptr);
	}
	
	return rh::result::ok;
}

