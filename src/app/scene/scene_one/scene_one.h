#pragma once

#include "../../rhi/rhi_frame.h"
#include "../../rhi/rhi_shader.h"
#include "../../camera.h"
#include "../scene.h"

class scene_one final : public scene
{
public:
	scene_one();
	rh::result deinit(rh::ctx& ctx) override;
	rh::result build(const rh::ctx& ctx) override;
	rh::result update(const rh::ctx& ctx) override;
	rh::result depth(rh::ctx ctx) override;
	rh::result draw(rh::ctx ctx) override;
	rh::result draw_ui(const rh::ctx& ctx) override;
private:
	gpu_scene_data scene_data_ = {};
	camera camera_;

	std::shared_ptr<mesh_scene> scene_graph_;
	std::shared_ptr<mesh_scene> skybox_;

	// Scene
	glm::vec3 scene_rot_ = glm::vec3(0.f);
	glm::vec3 scene_pos_ = glm::vec3(0.f, 2.5f, 2.5f);
	float scene_scale_ = 0.1f;
	glm::vec3 sunlight_direction_ = glm::vec3(2.f, 2.593f, -1.362f);
	glm::mat4 shadow_map_view_{ 1.f };

	bool toggle_wire_frame_ = false;
	bool light_view_ = false;
	int blend_mode_ = 0;
	float near_plane_ = 50.f;
	float distance_from_camera_ = 0.02f;

	void update_scene(const rh::ctx& ctx);
	std::vector<glm::vec4> shadow_map_frustum(const glm::mat4& proj, const glm::mat4& view);
	glm::mat4 shadow_map_view(const std::vector<glm::vec4>& shadow_frustum, const glm::vec3 light_direction);
	glm::mat4 shadow_map_projection(const std::vector<glm::vec4>& shadow_frustum, const glm::mat4& shadow_map_view);
	glm::mat4 shadow_map_projection(const glm::vec3 light_direction, const glm::mat4& p, const glm::mat4& world_view);
};