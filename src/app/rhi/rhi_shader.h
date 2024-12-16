#pragma once
#include "../../Rosy.h"
#include "rhi_types.h"
#include "rhi_helpers.h"
#include "rhi_cmd.h"

class rhi;

enum class shader_blending :uint8_t {
    blending_disabled,
    blending_additive,
    blending_alpha_blend,
};

class shader_pipeline {
public:
    shader_pipeline() = default;
    void deinit(const VkDevice device) const;

    void with_shaders(const std::vector<char>& vert, const std::vector<char>& frag);
    VkResult build(VkDevice device);
    VkResult shade(VkCommandBuffer cmd) const;
    VkResult push(VkCommandBuffer cmd) const;

    std::string name{ "geometry" };
    float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    std::vector<VkShaderEXT> shaders = {};
	std::optional<VkPipelineLayout> pipeline_layout = {};
    void* shader_constants = nullptr;
    uint32_t shader_constants_size = 0;
    std::vector<VkDescriptorSetLayout> layouts;

    VkExtent2D viewport_extent = {};
    bool depth_enabled = true;
    bool wire_frames_enabled = false;
    bool culling_enabled = true;
    VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    shader_blending blending = shader_blending::blending_disabled;
private:
    std::vector<VkShaderCreateInfoEXT> shaders_create_info_;
};