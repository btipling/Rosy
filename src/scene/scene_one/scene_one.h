#pragma once

#include "../../../rhi/rhi_frame.h"
#include "../../../rhi/rhi_shader.h"

class scene_one
{
public:
	rh::result build(const rh::ctx& ctx);
	rh::result draw(rh::ctx ctx);
	rh::result deinit(const rh::ctx& ctx) const;
private:
	std::optional <VkDescriptorSetLayout> single_image_descriptor_layout_;
	std::optional<shader_pipeline> test_mesh_pipeline_ = std::nullopt;
	float model_rot_x_ = 0.0f;
	float model_rot_y_ = 0.0f;
	float model_rot_z_ = 0.0f;
	float model_x_ = 0.0f;
	float model_y_ = 0.0f;
	float model_z_ = -15.0f;
	float model_scale_ = 1.0f;
	bool toggle_wire_frame_ = false;
	int blend_mode_ = 0;
};