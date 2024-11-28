#pragma once

class shader_builder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineLayout pipeline_layout;

    shader_builder() { clear(); }

    void clear();

    VkPipeline build_pipeline(VkDevice device);
};