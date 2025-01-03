#pragma once
#include "../../Rosy.h"

namespace rhi_cmd
{
	void set_rendering_defaults(VkCommandBuffer cmd, VkPrimitiveTopology primitive = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	void toggle_depth(VkCommandBuffer cmd, bool enable);
	void toggle_culling(VkCommandBuffer cmd, bool enable, VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE);
	void toggle_wire_frame(VkCommandBuffer cmd, bool enable, float line_width = 1.0);
	void set_view_port(VkCommandBuffer cmd, VkExtent2D extent);
	void disable_blending(VkCommandBuffer cmd);
	void enable_blending_additive(VkCommandBuffer cmd);
	void enable_blending_alpha_blend(VkCommandBuffer cmd);
}