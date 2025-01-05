#pragma once

#include "../../rhi/rhi_frame.h"
#include "../../rhi/rhi_shader.h"
#include "../scene.h"
class scene_one final : public scene
{
public:
	scene_one();
	rh::result deinit(rh::ctx& ctx) override;
	rh::result build(const rh::ctx& ctx) override;
	rh::result update(const rh::ctx& ctx) override;
	rh::result depth(rh::ctx ctx, int pass_number) override;
	rh::result draw(rh::ctx ctx) override;
	rh::result draw_ui(const rh::ctx& ctx) override;
private:

	std::shared_ptr<mesh_scene> scene_graph_;
	std::shared_ptr<mesh_scene> skybox_;

	// Scene
	//TODO: (skybox-fix) remove these
	glm::vec3 scene_rot_ = glm::vec3(0.f);
	glm::vec3 scene_pos_ = glm::vec3(0.f, 2.5f, 2.5f);
	float scene_scale_ = 0.1f;
	glm::mat4 shadow_map_view_{ 1.f };

	bool toggle_wire_frame_{ false };

	void update_scene(const rh::ctx& ctx);
};