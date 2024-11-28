#pragma once
#include "../Rosy.h"


enum class shader_blending :uint8_t {
    blending_disabled,
    blending_additive,
    blending_alpha_blend,
};

class shader_pipeline {
public:
    shader_pipeline() {}
    std::vector<VkShaderEXT> shaders = {};
	std::optional<VkPipelineLayout> pipeline_layout = {};

    VkExtent2D viewport_extent = {};
    VkColorBlendEquationEXT color_blend_equation_ext = {};
    bool depth_enabled = false;
    shader_blending blending = shader_blending::blending_disabled;

    VkResult build_shaders(VkDevice device);
private:
    std::vector<VkShaderCreateInfoEXT> shaders_create_info_;
};