#include "rhi_cmd.h"

namespace rhi_cmd
{
	void set_rendering_defaults(const VkCommandBuffer cmd, const VkPrimitiveTopology primitive) {

		{
			vkCmdSetRasterizerDiscardEnableEXT(cmd, VK_FALSE);
			vkCmdSetPrimitiveTopologyEXT(cmd, primitive);
			vkCmdSetDepthClipEnableEXT(cmd, VK_TRUE);
			vkCmdSetPrimitiveRestartEnableEXT(cmd, VK_FALSE);
			vkCmdSetRasterizationSamplesEXT(cmd, VK_SAMPLE_COUNT_1_BIT);
		}
		{
			constexpr VkSampleMask sample_mask = 0x1;
			vkCmdSetSampleMaskEXT(cmd, VK_SAMPLE_COUNT_1_BIT, &sample_mask);
		}
		{
			constexpr VkColorComponentFlags color_component_flags[] = { VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT };
			vkCmdSetColorWriteMaskEXT(cmd, 0, 1, color_component_flags);
		}
		{
			vkCmdSetVertexInputEXT(cmd, 0, nullptr, 0, nullptr);
		}
		return;
	}

	void toggle_depth(const VkCommandBuffer cmd, const bool enable)
	{
		vkCmdSetDepthClampEnableEXT(cmd, false);
		vkCmdSetDepthTestEnableEXT(cmd, enable);
		vkCmdSetDepthWriteEnableEXT(cmd, enable);
		vkCmdSetDepthCompareOpEXT(cmd, VK_COMPARE_OP_GREATER_OR_EQUAL);
		vkCmdSetDepthBoundsTestEnableEXT(cmd, VK_FALSE);
		vkCmdSetDepthBiasEnableEXT(cmd, VK_FALSE);
		vkCmdSetStencilTestEnableEXT(cmd, VK_FALSE);
		//vkCmdSetLogicOpEnableEXT(cmd, VK_TRUE);
		vkCmdSetDepthBounds(cmd, 0.0f, 1.0f);
		vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);
		return;
	}

	void toggle_culling(const VkCommandBuffer cmd, const bool enable, const VkFrontFace front_face) {
		vkCmdSetFrontFaceEXT(cmd, front_face);
		vkCmdSetCullModeEXT(cmd, enable);
		return;
	}

	void toggle_wire_frame(const VkCommandBuffer cmd, const bool enable, const float line_width) {
		if (enable) {
			vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_LINE);
			vkCmdSetLineWidth(cmd, line_width);
			return;
		}
		vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);
		return;
	}

	void set_view_port(const VkCommandBuffer cmd, const VkExtent2D extent) {
		{
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(extent.width);
			viewport.height = static_cast<float>(extent.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetViewportWithCountEXT(cmd, 1, &viewport);
		}
		{
			VkRect2D scissor = {};
			scissor.offset = { 0, 0 };
			scissor.extent = extent;
			vkCmdSetScissor(cmd, 0, 1, &scissor);
			vkCmdSetScissorWithCountEXT(cmd, 1, &scissor);
		}
		return;
	}

	void disable_blending(const VkCommandBuffer cmd) {
		constexpr VkColorBlendEquationEXT color_blend_equation_ext{};
		vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &color_blend_equation_ext);
		constexpr VkBool32 enable = VK_FALSE;
		vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);
		return;
	}

	void enable_blending_additive(const VkCommandBuffer cmd) {
		constexpr VkColorComponentFlags flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &flags);
		constexpr VkBool32 enable = VK_TRUE;
		vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);
		VkColorBlendEquationEXT blend_config = {};
		blend_config.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blend_config.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_config.colorBlendOp = VK_BLEND_OP_ADD;
		blend_config.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_config.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend_config.alphaBlendOp = VK_BLEND_OP_ADD;
		vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &blend_config);
		return;
	}

	void enable_blending_alpha_blend(const VkCommandBuffer cmd) {
		constexpr VkColorComponentFlags flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &flags);
		constexpr VkBool32 enable = VK_TRUE;
		vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);
		VkColorBlendEquationEXT blend_config = {};
		blend_config.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blend_config.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blend_config.colorBlendOp = VK_BLEND_OP_ADD;
		blend_config.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_config.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend_config.alphaBlendOp = VK_BLEND_OP_ADD;
		vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &blend_config);
		return;
	}
}