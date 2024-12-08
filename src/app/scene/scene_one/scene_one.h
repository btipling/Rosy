#pragma once

#include "../../rhi/rhi_frame.h"
#include "../../rhi/rhi_shader.h"
#include "../../camera.h"

class scene_one
{
public:
	scene_one() :
	camera_(glm::vec3{ .0f, 0.0f, -20.0f }){}
	rh::result build(const rh::ctx& ctx);
	rh::result update(const rh::ctx& ctx);
	rh::result draw(rh::ctx ctx);
	rh::result draw_ui(const rh::ctx& ctx);
	rh::result deinit(rh::ctx& ctx);
private:
	void update_scene(const rh::ctx& ctx);
	gpu_scene_data scene_data_ = {};
	camera camera_;

	std::optional<VkSampler> image_sampler_ = std::nullopt;
	std::optional<VkSampler> skybox_sampler_ = std::nullopt;

	std::optional<VkDescriptorSetLayout> gpu_scene_data_descriptor_layout_ = std::nullopt;

	std::vector<std::shared_ptr<mesh_asset>> scene_graph_;

	// Earth
	std::optional <VkDescriptorSetLayout> earth_image_descriptor_layout_ = std::nullopt;
	std::optional<VkDescriptorSet> earth_image_descriptor_set_ = std::nullopt;
	std::optional<shader_pipeline> earth_pipeline_ = std::nullopt;
	std::optional<ktxTexture*> earth_texture_ = std::nullopt;
	std::optional<ktxVulkanTexture> earth_vk_texture_ = std::nullopt;
	std::optional<VkImageView> earth_view_ = std::nullopt;
	glm::vec3 earth_rot_ = glm::vec3(0.f);
	glm::vec3 earth_pos_ = glm::vec3(0.f, 0.f, -15.f);
	float earth_scale_ = 1.0f;

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