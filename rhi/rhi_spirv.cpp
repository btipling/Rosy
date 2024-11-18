#include "RHI.h"

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}



VkResult CreateShadersEXT(
    VkInstance instance,
    VkDevice device,
    uint32_t createInfoCount,
    const VkShaderCreateInfoEXT* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkShaderEXT* pShaders) {
    auto func = (PFN_vkCreateShadersEXT)vkGetInstanceProcAddr(instance, "vkCreateShadersEXT");
    if (func != nullptr) {
        return func(device, createInfoCount, pCreateInfos, pAllocator, pShaders);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}


// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_shader_object.html
//// Logical device created with the shaderObject feature enabled
//VkDevice device;
//
//// SPIR-V shader code for a vertex shader, along with its size in bytes
//void* pVertexSpirv;
//size_t vertexSpirvSize;
//
//// SPIR-V shader code for a fragment shader, along with its size in bytes
//void* pFragmentSpirv;
//size_t fragmentSpirvSize;
//
//// Descriptor set layout compatible with the shaders
//VkDescriptorSetLayout descriptorSetLayout;
//
//VkShaderCreateInfoEXT shaderCreateInfos[2] =
//{
//    {
//        .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
//        .pNext = NULL,
//        .flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,
//        .stage = VK_SHADER_STAGE_VERTEX_BIT,
//        .nextStage = VK_SHADER_STAGE_FRAGMENT_BIT,
//        .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
//        .codeSize = vertexSpirvSize,
//        .pCode = pVertexSpirv,
//        .pName = "main",
//        .setLayoutCount = 1,
//        .pSetLayouts = &descriptorSetLayout;
//        .pushConstantRangeCount = 0,
//        .pPushConstantRanges = NULL,
//        .pSpecializationInfo = NULL
//    },
//    {
//        .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
//        .pNext = NULL,
//        .flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,
//        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
//        .nextStage = 0,
//        .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
//        .codeSize = fragmentSpirvSize,
//        .pCode = pFragmentSpirv,
//        .pName = "main",
//        .setLayoutCount = 1,
//        .pSetLayouts = &descriptorSetLayout;
//        .pushConstantRangeCount = 0,
//        .pPushConstantRanges = NULL,
//        .pSpecializationInfo = NULL
//    }
//};
//
//VkResult result;
//VkShaderEXT shaders[2];
//
//result = vkCreateShadersEXT(device, 2, &shaderCreateInfos, NULL, shaders);
//if (result != VK_SUCCESS)
//{
//    // Handle error
//}

//// Command buffer in the recording state
//VkCommandBuffer commandBuffer;
//
//// Vertex and fragment shader objects created above
//VkShaderEXT shaders[2];
//
//// Assume vertex buffers, descriptor sets, etc. have been bound, and existing
//// state setting commands have been called to set all required state
//
//const VkShaderStageFlagBits stages[2] =
//{
//    VK_SHADER_STAGE_VERTEX_BIT,
//    VK_SHADER_STAGE_FRAGMENT_BIT
//};
//
//// Bind linked shaders
//vkCmdBindShadersEXT(commandBuffer, 2, stages, shaders);
//
//// Equivalent to the previous line. Linked shaders can be bound one at a time,
//// in any order:
//// vkCmdBindShadersEXT(commandBuffer, 1, &stages[1], &shaders[1]);
//// vkCmdBindShadersEXT(commandBuffer, 1, &stages[0], &shaders[0]);
//
//// The above is sufficient to draw if the device was created with the
//// tessellationShader and geometryShader features disabled. Otherwise, since
//// those stages should not execute, vkCmdBindShadersEXT() must be called at
//// least once with each of their stages in pStages before drawing:
//
//const VkShaderStageFlagBits unusedStages[3] =
//{
//    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
//    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
//    VK_SHADER_STAGE_GEOMETRY_BIT
//};
//
//// NULL pShaders is equivalent to an array of stageCount VK_NULL_HANDLE values,
//// meaning no shaders are bound to those stages, and that any previously bound
//// shaders are unbound
//vkCmdBindShadersEXT(commandBuffer, 3, unusedStages, NULL);
//
//// Graphics shader objects may only be used to draw inside dynamic render pass
//// instances begun with vkCmdBeginRendering(), assume one has already been begun
//
//// Draw a triangle
//vkCmdDraw(commandBuffer, 3, 1, 0, 0);

// https://docs.vulkan.org/samples/latest/samples/extensions/dynamic_rendering/README.html
//VkRenderingAttachmentInfoKHR color_attachment_info = vkb::initializers::rendering_attachment_info();
//color_attachment_info.imageView = swapchain_buffers[i].view;        // color_attachment.image_view;
//...
//
//VkRenderingAttachmentInfoKHR depth_attachment_info = vkb::initializers::rendering_attachment_info();
//depth_attachment_info.imageView = depth_stencil.view;
//...
//
//auto render_area = VkRect2D{ VkOffset2D{}, VkExtent2D{width, height} };
//auto render_info = vkb::initializers::rendering_info(render_area, 1, &color_attachment_info);
//render_info.layerCount = 1;
//render_info.pDepthAttachment = &depth_attachment_info;
//render_info.pStencilAttachment = &depth_attachment_info;
//
//vkCmdBeginRenderingKHR(draw_cmd_buffer, &render_info);
// 
// 
//draw_scene();
// 
// 
// 
// 
//vkCmdEndRenderingKHR(draw_cmd_buffer);