#pragma once

#include "../../rhi/rhi_frame.h"
#include "../../rhi/rhi_shader.h"
#include "../../camera.h"
#include "../scene.h"

class scene_two final : public scene
{
public:
	scene_two() :
		camera_(glm::vec3{ 0.2f, 0.6f, -1.3f }) {
		camera_.pitch = 0.36f;
		camera_.yaw = 1.f;
	}
	rh::result build(const rh::ctx& ctx) override;
	rh::result update(const rh::ctx& ctx) override;
	rh::result draw(rh::ctx ctx) override;
	rh::result draw_ui(const rh::ctx& ctx) override;
	rh::result deinit(rh::ctx& ctx) override;
private:
	void update_scene(const rh::ctx& ctx);
	gpu_scene_data scene_data_ = {};
	camera camera_;

	std::optional<VkSampler> image_sampler_ = std::nullopt;
	std::optional<VkSampler> skybox_sampler_ = std::nullopt;

	std::optional<VkDescriptorSetLayout> gpu_scene_data_descriptor_layout_ = std::nullopt;

	std::shared_ptr<mesh_scene> scene_graph_;

	// Scene
	std::optional <VkDescriptorSetLayout> scene_image_descriptor_layout_ = std::nullopt;
	std::optional<VkDescriptorSet> scene_image_descriptor_set_ = std::nullopt;
	std::optional<shader_pipeline> scene_pipeline_ = std::nullopt;
	std::optional<ktxTexture*> scene_texture_ = std::nullopt;
	std::optional<ktxVulkanTexture> scene_vk_texture_ = std::nullopt;
	std::optional<VkImageView> scene_view_ = std::nullopt;
	glm::vec3 scene_rot_ = glm::vec3(0.f);
	glm::vec3 scene_pos_ = glm::vec3(0.f, 0.f, 0.f);
	float scene_scale_ = 0.1f;

	glm::vec3 sunlight_direction_ = glm::vec3(2.f, 2.593f, -1.362f);

	// Skybox
	std::optional <VkDescriptorSetLayout> skybox_image_descriptor_layout_ = std::nullopt;
	std::optional<VkDescriptorSet> skybox_image_descriptor_set_ = std::nullopt;
	std::optional<shader_pipeline> skybox_pipeline_ = std::nullopt;
	std::optional<ktxTexture*> skybox_texture_ = std::nullopt;
	std::optional<ktxVulkanTexture> skybox_vk_texture_ = std::nullopt;
	std::optional<VkImageView> skybox_view_ = std::nullopt;

	bool toggle_wire_frame_ = false;
	int blend_mode_ = 0;
};