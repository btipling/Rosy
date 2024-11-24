#include "RHI.h"

void Rhi::toggleDepth(VkCommandBuffer cmd, bool enable) {
	vkCmdSetDepthTestEnableEXT(cmd, enable);
	vkCmdSetDepthWriteEnableEXT(cmd, enable);
	vkCmdSetDepthCompareOpEXT(cmd, VK_COMPARE_OP_GREATER_OR_EQUAL);
	vkCmdSetDepthBoundsTestEnableEXT(cmd, VK_FALSE);
	vkCmdSetDepthBiasEnableEXT(cmd, VK_FALSE);
	vkCmdSetStencilTestEnableEXT(cmd, VK_FALSE);
	vkCmdSetLogicOpEnableEXT(cmd, VK_FALSE);
	vkCmdSetDepthBounds(cmd, 0.0f, 1.0f);
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