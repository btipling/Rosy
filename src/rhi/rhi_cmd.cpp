#include "RHI.h"

void Rhi::setRenderingDefaults(VkCommandBuffer cmd) {

	{
		vkCmdSetRasterizerDiscardEnableEXT(cmd, VK_FALSE);
		vkCmdSetPrimitiveTopologyEXT(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		vkCmdSetPrimitiveRestartEnableEXT(cmd, VK_FALSE);
		vkCmdSetRasterizationSamplesEXT(cmd, VK_SAMPLE_COUNT_1_BIT);
	}
	{
		const VkSampleMask sample_mask = 0x1;
		vkCmdSetSampleMaskEXT(cmd, VK_SAMPLE_COUNT_1_BIT, &sample_mask);
	}
	{
		VkColorComponentFlags color_component_flags[] = { VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT };
		vkCmdSetColorWriteMaskEXT(cmd, 0, 1, color_component_flags);
	}
	{
		vkCmdSetVertexInputEXT(cmd, 0, nullptr, 0, nullptr);
	}
}

void Rhi::toggleDepth(VkCommandBuffer cmd, bool enable) {
	vkCmdSetDepthTestEnableEXT(cmd, enable);
	vkCmdSetDepthWriteEnableEXT(cmd, enable);
	vkCmdSetDepthCompareOpEXT(cmd, VK_COMPARE_OP_GREATER_OR_EQUAL);
	vkCmdSetDepthBoundsTestEnableEXT(cmd, VK_FALSE);
	vkCmdSetDepthBiasEnableEXT(cmd, VK_FALSE);
	vkCmdSetStencilTestEnableEXT(cmd, VK_FALSE);
	vkCmdSetLogicOpEnableEXT(cmd, VK_FALSE);
	vkCmdSetDepthBounds(cmd, 0.0f, 1.0f);
	vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);
}

void Rhi::toggleCulling(VkCommandBuffer cmd, bool enable) {
	vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	vkCmdSetCullModeEXT(cmd, enable);
}

void Rhi::toggleWireFrame(VkCommandBuffer cmd, bool enable) {
	if (enable) {
		vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_LINE);
		return;
	}
	vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);
}

void Rhi::setViewPort(VkCommandBuffer cmd, VkExtent2D extent) {
	{
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)extent.width;
		viewport.height = (float)extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetViewportWithCountEXT(cmd, 1, &viewport);
	}
	{
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdSetScissorWithCountEXT(cmd, 1, &scissor);
	}
}

void Rhi::disableBlending(VkCommandBuffer cmd) {
	VkColorBlendEquationEXT colorBlendEquationEXT{};
	vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &colorBlendEquationEXT);
	VkBool32 enable = VK_FALSE;
	vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);
}

void Rhi::enableBlendingAdditive(VkCommandBuffer cmd) {
	VkColorComponentFlags flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &flags);
	VkBool32 enable = VK_TRUE;
	vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);
	VkColorBlendEquationEXT blendConfig = {};
	blendConfig.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendConfig.colorBlendOp = VK_BLEND_OP_ADD;
	blendConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendConfig.alphaBlendOp = VK_BLEND_OP_ADD;
	vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &blendConfig);
}

void Rhi::enableBlendingAlphaBlend(VkCommandBuffer cmd) {
	VkColorComponentFlags flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &flags);
	VkBool32 enable = VK_TRUE;
	vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);
	VkColorBlendEquationEXT blendConfig = {};
	blendConfig.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendConfig.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendConfig.colorBlendOp = VK_BLEND_OP_ADD;
	blendConfig.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendConfig.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendConfig.alphaBlendOp = VK_BLEND_OP_ADD;
	vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &blendConfig);
}
