#pragma once

#include "../../rhi/rhi_frame.h"
#include "../../rhi/rhi_shader.h"
#include "../../camera.h"
#include "../scene.h"

class scene_two final : public scene
{
public:
	scene_two();
	rh::result build(const rh::ctx& ctx) override;
	rh::result update(const rh::ctx& ctx) override;
	rh::result depth(rh::ctx ctx) override;
	rh::result draw(rh::ctx ctx) override;
	rh::result draw_ui(const rh::ctx& ctx) override;
	rh::result deinit(rh::ctx& ctx) override;
private:
	void update_scene(const rh::ctx& ctx);
	gpu_scene_data scene_data_ = {};
	camera camera_;

	std::optional<VkDescriptorSetLayout> gpu_scene_data_descriptor_layout_ = std::nullopt;
	std::shared_ptr<mesh_scene> scene_graph_;
	std::shared_ptr<mesh_scene> skybox_;

	// Scene
	glm::vec3 scene_rot_ = glm::vec3(0.f);
	glm::vec3 scene_pos_ = glm::vec3(0.f, 0.f, 0.f);
	float scene_scale_ = 0.1f;
	glm::vec3 sunlight_direction_ = glm::vec3(2.f, 2.593f, -1.362f);

	bool toggle_wire_frame_ = false;
	int blend_mode_ = 0;
};