#include "scene_one.h"
#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>
#include "../../utils/utils.h"
#include "../../loader/loader.h"


rh::result scene_one::build(const rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	auto data = ctx.rhi.data.value();
	std::vector<VkDescriptorSetLayout> layouts;
	descriptor_allocator_growable descriptor_allocator = ctx.rhi.descriptor_allocator.value();
	{
		descriptor_layout_builder layout_builder;
		layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return rh::result::error;
		gpu_scene_data_descriptor_layout_ = set;
		layouts.push_back(set);
	}
	{
		descriptor_layout_builder layout_builder;
		layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return rh::result::error;
		earth_image_descriptor_layout_ = set;
		layouts.push_back(set);
	}
	{
		descriptor_layout_builder layout_builder;
		layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		auto [result, set] = layout_builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return rh::result::error;
		skybox_image_descriptor_layout_ = set;
		layouts.push_back(set);
	}
	std::vector<char> earth_vertex_shader;
	std::vector<char> earth_fragment_shader;
	std::vector<char> skybox_vertex_shader;
	std::vector<char> skybox_fragment_shader;
	try
	{
		earth_vertex_shader = read_file("out/mesh.vert.spv");
		earth_fragment_shader = read_file("out/mesh.frag.spv");
		skybox_vertex_shader = read_file("out/skybox.vert.spv");
		skybox_fragment_shader = read_file("out/skybox.frag.spv");
	}
	catch (const std::exception& e)
	{
		rosy_utils::debug_print_a("error reading shader files! %s", e.what());
		return rh::result::error;
	}

	{
		// Earth pipeline
		shader_pipeline sp = {};
		sp.layouts = layouts;
		sp.name = "earth";
		sp.with_shaders(earth_vertex_shader, earth_fragment_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return rh::result::error;
		earth_pipeline_ = sp;
	}

	{
		// Skybox pipeline
		shader_pipeline sp = {};
		sp.layouts = layouts;
		sp.name = "skybox";
		sp.with_shaders(skybox_vertex_shader, skybox_fragment_shader);
		if (const VkResult result = sp.build(ctx.rhi.device); result != VK_SUCCESS) return rh::result::error;
		skybox_pipeline_ = sp;
	}


	// ReSharper disable once StringLiteralTypo
	if (auto load_result = data->load_gltf_meshes("assets\\sphere.glb"); load_result.has_value())
	{
		scene_graph_ = std::make_shared<mesh_scene>(std::move(load_result.value()));
	}
	else
	{
		return rh::result::error;
	}

	const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));

	{
		// Earth texture and sampler
		{

			ktxTexture* k_texture;
			ktx_error_code_e ktx_result = ktxTexture_CreateFromNamedFile("assets/earth_4k.ktx2",
				KTX_TEXTURE_CREATE_NO_FLAGS,
				&k_texture);
			if (ktx_result != KTX_SUCCESS) {
				rosy_utils::debug_print_a("ktx read failure: %d\n", ktx_result);
				return rh::result::error;
			}
			earth_texture_ = k_texture;
			if (std::expected<ktxVulkanTexture, ktx_error_code_e> res = data->create_image(k_texture, VK_IMAGE_USAGE_SAMPLED_BIT); res.has_value())
			{
				earth_vk_texture_ = res.value();
			}
			else
			{
				rosy_utils::debug_print_a("ktx upload failure: %d\n", res.error());
			}


			VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;

			VkImageViewCreateInfo view_info = rhi_helpers::img_view_create_info(earth_vk_texture_.value().imageFormat, earth_vk_texture_.value().image, aspect_flag);
			view_info.subresourceRange.levelCount = earth_vk_texture_.value().levelCount;
			view_info.subresourceRange.layerCount = earth_vk_texture_.value().layerCount;
			VkImageView img_view{};
			if (VkResult result = vkCreateImageView(device, &view_info, nullptr, &img_view); result !=
				VK_SUCCESS)
			{
				return rh::result::error;
			}
			earth_view_ = img_view;
		}
		{
			VkSamplerCreateInfo sample = {};
			sample.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sample.maxLod = earth_vk_texture_.value().levelCount;
			sample.minLod = 0;

			sample.magFilter = VK_FILTER_LINEAR;
			sample.minFilter = VK_FILTER_LINEAR;

			sample.anisotropyEnable = VK_FALSE;
			sample.maxAnisotropy = 0.f;

			// Set proper address modes for spherical mapping
			sample.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sample.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sample.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			// Linear mipmap mode instead of NEAREST
			sample.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			VkSampler sampler{};
			if (VkResult result = vkCreateSampler(device, &sample, nullptr, &sampler); result != VK_SUCCESS) return rh::result::error;
			image_sampler_ = sampler;
		}
		{
			auto [image_set_result, image_set] = descriptor_allocator.allocate(device, earth_image_descriptor_layout_.value());
			if (image_set_result != VK_SUCCESS) return  rh::result::error;
			{
				descriptor_writer writer;
				writer.write_image(0, earth_view_.value(), image_sampler_.value(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				writer.update_set(device, image_set);
				earth_image_descriptor_set_ = image_set;
			}
		}
	}

	{
		// Skybox texture and sampler
		{

			ktxTexture* k_texture;
			ktx_error_code_e ktx_result = ktxTexture_CreateFromNamedFile("assets/skybox_milky_way.ktx2",
				KTX_TEXTURE_CREATE_NO_FLAGS,
				&k_texture);
			if (ktx_result != KTX_SUCCESS) {
				rosy_utils::debug_print_a("ktx read failure: %d\n", ktx_result);
				return rh::result::error;
			}

			skybox_texture_ = k_texture;
			if (std::expected<ktxVulkanTexture, ktx_error_code_e> res = data->create_image(k_texture, VK_IMAGE_USAGE_SAMPLED_BIT); res.has_value())
			{
				skybox_vk_texture_ = res.value();
			}
			else
			{
				rosy_utils::debug_print_a("ktx upload failure: %d\n", res.error());
			}


			VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;

			VkImageViewCreateInfo view_info = rhi_helpers::img_view_create_info(skybox_vk_texture_.value().imageFormat, skybox_vk_texture_.value().image, aspect_flag);
			view_info.subresourceRange.levelCount = skybox_vk_texture_.value().levelCount;
			view_info.subresourceRange.layerCount = skybox_vk_texture_.value().layerCount;
			view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			VkImageView img_view{};
			if (VkResult result = vkCreateImageView(device, &view_info, nullptr, &img_view); result !=
				VK_SUCCESS)
			{
				return rh::result::error;
			}
			skybox_view_ = img_view;
		}
		{
			VkSamplerCreateInfo sample = {};
			sample.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sample.maxLod = skybox_vk_texture_.value().levelCount;
			sample.minLod = 0;

			sample.magFilter = VK_FILTER_LINEAR;
			sample.minFilter = VK_FILTER_LINEAR;

			sample.anisotropyEnable = VK_FALSE;
			sample.maxAnisotropy = 0.f;
			sample.mipLodBias = 0.0f;
			sample.compareOp = VK_COMPARE_OP_NEVER;

			// Set proper address modes for spherical mapping
			sample.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sample.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sample.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

			// Linear mipmap mode instead of NEAREST
			sample.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			VkSampler sampler{};
			if (VkResult result = vkCreateSampler(device, &sample, nullptr, &sampler); result != VK_SUCCESS) return rh::result::error;
			skybox_sampler_ = sampler;
		}
		{
			auto [image_set_result, image_set] = descriptor_allocator.allocate(device, skybox_image_descriptor_layout_.value());
			if (image_set_result != VK_SUCCESS) return  rh::result::error;
			{
				descriptor_writer writer;
				writer.write_image(0, skybox_view_.value(), skybox_sampler_.value(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				writer.update_set(device, image_set);
				skybox_image_descriptor_set_ = image_set;
			}
		}
	}



	return rh::result::ok;
}

rh::result scene_one::draw(rh::ctx ctx)
{
	update_scene(ctx);
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
	shader_pipeline earth_shaders = earth_pipeline_.value();
	shader_pipeline skybox_shaders = skybox_pipeline_.value();
	VkExtent2D frame_extent = ctx.rhi.frame_extent;
	VmaAllocator allocator = ctx.rhi.allocator;

	// Set descriptor sets
	std::vector<VkDescriptorSet> earth_sets;
	std::vector<VkDescriptorSet> skybox_sets;
	{
		// Global descriptor
		auto [desc_result, desc_set] = frame_descriptors.allocate(device, gpu_scene_data_descriptor_layout_.value());
		if (desc_result != VK_SUCCESS) return rh::result::error;
		void* data_pointer;
		vmaMapMemory(allocator, gpu_scene_buffer.allocation, &data_pointer);
		memcpy(data_pointer, &scene_data_, sizeof(scene_data_));
		vmaUnmapMemory(allocator, gpu_scene_buffer.allocation);

		{
			VkDescriptorSet earth_descriptor = desc_set;
			// Earth
			descriptor_writer writer;
			writer.write_buffer(0, gpu_scene_buffer.buffer, sizeof(gpu_scene_data), 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			writer.update_set(device, earth_descriptor);
			earth_sets.push_back(desc_set);
			earth_sets.push_back(earth_image_descriptor_set_.value());
		}
		{
			VkDescriptorSet skybox_descriptor = desc_set;
			// Skybox
			descriptor_writer writer;
			writer.write_buffer(0, gpu_scene_buffer.buffer, sizeof(gpu_scene_data), 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			writer.update_set(device, skybox_descriptor);
			skybox_sets.push_back(desc_set);
			skybox_sets.push_back(skybox_image_descriptor_set_.value());
		}
	}
	{
		// Skybox
		{
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_shaders.pipeline_layout.value(), 0, skybox_sets.size(), skybox_sets.data(), 0, nullptr);
			float color[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
			VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label("skybox", color);
			vkCmdBeginDebugUtilsLabelEXT(cmd, &mesh_draw_label);

			gpu_draw_push_constants push_constants{};
			auto m = glm::mat4(1.0f);
			m = translate(m, camera_.position);
			push_constants.world_matrix = m;

			if (scene_graph_->meshes.size() > 0)
			{
				size_t mesh_index = 1;
				auto mesh = scene_graph_->meshes[mesh_index];
				push_constants.vertex_buffer = mesh->mesh_buffers.vertex_buffer_address;
				skybox_shaders.viewport_extent = frame_extent;
				skybox_shaders.shader_constants = &push_constants;
				skybox_shaders.shader_constants_size = sizeof(push_constants);
				skybox_shaders.wire_frames_enabled = toggle_wire_frame_;
				skybox_shaders.depth_enabled = false;
				skybox_shaders.front_face = VK_FRONT_FACE_CLOCKWISE;
				skybox_shaders.blending = static_cast<shader_blending>(blend_mode_);
				if (VkResult result = skybox_shaders.shade(cmd); result != VK_SUCCESS) return rh::result::error;
				if (VkResult result = skybox_shaders.push(cmd); result != VK_SUCCESS) return rh::result::error;
				vkCmdBindIndexBuffer(cmd, mesh->mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmd, mesh->surfaces[0].count, 1, mesh->surfaces[0].start_index,
					0, 0);
			}
			vkCmdEndDebugUtilsLabelEXT(cmd);
		}
		// Earth
		{
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, earth_shaders.pipeline_layout.value(), 0, earth_sets.size(), earth_sets.data(), 0, nullptr);
			float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
			VkDebugUtilsLabelEXT mesh_draw_label = rhi_helpers::create_debug_label("earth", color);
			vkCmdBeginDebugUtilsLabelEXT(cmd, &mesh_draw_label);

			gpu_draw_push_constants push_constants{};
			auto m = glm::mat4(1.0f);
			m = translate(m, earth_pos_);
			m = rotate(m, earth_rot_[0], glm::vec3(1, 0, 0));
			m = rotate(m, earth_rot_[1], glm::vec3(0, 1, 0));
			m = rotate(m, earth_rot_[2], glm::vec3(0, 0, 1));
			m = scale(m, glm::vec3(earth_scale_, earth_scale_, earth_scale_));
			m = scale(m, glm::vec3(earth_scale_, earth_scale_, earth_scale_));
			push_constants.world_matrix = m;

			if (scene_graph_->meshes.size() > 0)
			{
				size_t mesh_index = 0;
				auto mesh = scene_graph_->meshes[mesh_index];
				push_constants.vertex_buffer = mesh->mesh_buffers.vertex_buffer_address;
				earth_shaders.viewport_extent = frame_extent;
				earth_shaders.shader_constants = &push_constants;
				earth_shaders.shader_constants_size = sizeof(push_constants);
				earth_shaders.wire_frames_enabled = toggle_wire_frame_;
				earth_shaders.depth_enabled = true;
				earth_shaders.blending = static_cast<shader_blending>(blend_mode_);
				if (VkResult result = earth_shaders.shade(cmd); result != VK_SUCCESS) return rh::result::error;
				if (VkResult result = earth_shaders.push(cmd); result != VK_SUCCESS) return rh::result::error;
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
	ImGui::Begin("Solar System");
	{
		ImGui::Text("Camera position: (%f, %f, %f)", camera_.position.x, camera_.position.y, camera_.position.z);
		ImGui::SliderFloat3("Rotate", value_ptr(earth_rot_), 0, glm::pi<float>() * 2.0f);
		ImGui::SliderFloat3("Translate", value_ptr(earth_pos_), -100.0f, 100.0f);
		ImGui::SliderFloat("Scale", &earth_scale_, 0.1f, 10.0f);
		ImGui::SliderFloat3("Sunlight direction", value_ptr(sunlight_direction_), -30.0f, 30.0f);
		ImGui::Checkbox("Wireframe", &toggle_wire_frame_);
		ImGui::Text("Blending");
		ImGui::RadioButton("disabled", &blend_mode_, 0); ImGui::SameLine();
		ImGui::RadioButton("additive", &blend_mode_, 1); ImGui::SameLine();
		ImGui::RadioButton("alpha blend", &blend_mode_, 2);
	}
	ImGui::End();
	return rh::result::ok;
}

rh::result scene_one::deinit(rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	const VmaAllocator allocator = ctx.rhi.allocator;
	const auto buffer = ctx.rhi.data.value();
	{
		vkDeviceWaitIdle(device);
	}
	if (earth_view_.has_value())
	{
		vkDestroyImageView(device, earth_view_.value(), nullptr);
	}
	if (earth_vk_texture_.has_value())
	{
		ktxVulkanTexture_Destruct(&earth_vk_texture_.value(), device, nullptr);
	}
	if (earth_texture_.has_value())
	{
		ktxTexture_Destroy(earth_texture_.value());
	}


	if (skybox_view_.has_value())
	{
		vkDestroyImageView(device, skybox_view_.value(), nullptr);
	}
	if (skybox_vk_texture_.has_value())
	{
		ktxVulkanTexture_Destruct(&skybox_vk_texture_.value(), device, nullptr);
	}
	if (skybox_texture_.has_value())
	{
		ktxTexture_Destroy(skybox_texture_.value());
	}


	{
		if (ctx.rhi.descriptor_allocator.has_value())
		{
			ctx.rhi.descriptor_allocator.value().clear_pools(device);
		}
		if (image_sampler_.has_value()) vkDestroySampler(device, image_sampler_.value(), nullptr);
		if (skybox_sampler_.has_value()) vkDestroySampler(device, skybox_sampler_.value(), nullptr);
	}

	for (std::shared_ptr<mesh_asset> mesh : scene_graph_->meshes)
	{
		gpu_mesh_buffers rectangle = mesh.get()->mesh_buffers;
		buffer->destroy_buffer(rectangle.vertex_buffer);
		buffer->destroy_buffer(rectangle.index_buffer);
		mesh.reset();
	}
	if (earth_pipeline_.has_value()) {
		earth_pipeline_.value().deinit(device);
	}
	if (skybox_pipeline_.has_value()) {
		skybox_pipeline_.value().deinit(device);
	}
	if (gpu_scene_data_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device, gpu_scene_data_descriptor_layout_.value(), nullptr);
	}
	if (earth_image_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device, earth_image_descriptor_layout_.value(), nullptr);
	}
	if (skybox_image_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device, skybox_image_descriptor_layout_.value(), nullptr);
	}

	return rh::result::ok;
}

rh::result scene_one::update(const rh::ctx& ctx)
{
	camera_.process_sdl_event(ctx);
	return rh::result::ok;
}

void scene_one::update_scene(const rh::ctx& ctx)
{
	camera_.process_sdl_event(ctx);
	camera_.update(ctx);
	const auto [width, height] = ctx.rhi.frame_extent;

	constexpr float z_near = 0.1f;
	constexpr float z_far = 1000.0f;
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	constexpr float fov = glm::radians(70.0f);
	const float h = 1.0 / tan(fov * 0.5);
	const float w = h / aspect;
	constexpr float a = -z_near / (z_far - z_near);
	constexpr float b = (z_near * z_far) / (z_far - z_near);

	glm::mat4 proj(0.0f);

	proj[0][0] = w;
	proj[1][1] = -h;

	proj[2][2] = a;
	proj[3][2] = b;
	proj[2][3] = 1.0f;

	const glm::mat4 view = camera_.get_view_matrix();
	scene_data_.view = view;
	scene_data_.proj = proj;
	scene_data_.view_projection = proj * view;
	scene_data_.camera_position = glm::vec4(camera_.position, 1.f);
	scene_data_.ambient_color = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
	scene_data_.sunlight_direction = glm::vec4(sunlight_direction_, 0.0);
	scene_data_.sunlight_color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
}

