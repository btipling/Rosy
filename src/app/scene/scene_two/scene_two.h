#pragma once

#include "../../rhi/rhi_frame.h"
#include "../../rhi/rhi_shader.h"
#include "../scene.h"

enum class camera_view :int{ camera, csm, light };

class scene_two final : public scene
{
public:
	scene_two();
	rh::result deinit(rh::ctx& ctx) override;
	rh::result build(const rh::ctx& ctx) override;
	rh::result update(const rh::ctx& ctx) override;
	rh::result depth(rh::ctx ctx, int pass_number) override;
	rh::result draw(rh::ctx ctx) override;
	rh::result draw_ui(const rh::ctx& ctx) override;
private:
	gpu_scene_data scene_data_ = {};

	std::shared_ptr<mesh_scene> scene_graph_;
	std::shared_ptr<mesh_scene> skybox_;

	// Scene
	glm::vec3 scene_rot_ = glm::vec3(0.f);
	glm::vec3 scene_pos_ = glm::vec3(0.f, 2.5f, 2.5f);
	float scene_scale_ = 0.1f;
	glm::mat4 shadow_map_view_{ 1.f };

	bool toggle_wire_frame_{ false };
	camera_view current_view_{ camera_view::camera };
	int blend_mode_{ 0 };
	int near_plane_{ 0 };
	float distance_from_camera_{ 0.02f };

	void update_scene(const rh::ctx& ctx);
};