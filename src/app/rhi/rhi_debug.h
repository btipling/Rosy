#pragma once
#include "rhi_types.h"

struct debug_frustum
{
	std::array<glm::vec4, 8> points;
	static debug_frustum from_bounds(float min_x, float max_x, float min_y, float max_y, float min_z, float max_z);
};

class debug_gfx
{
public:
	std::string name{ "debug" };
	float color[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
	std::string vertex_path{ "out/debug.spv" };
	std::string frag_path{ "out/debug.spv" };

	std::optional<shader_pipeline> shaders = std::nullopt;
	std::vector<debug_draw_push_constants> lines;
	std::vector<debug_draw_push_constants> sunlight_lines;
	std::vector<debug_draw_push_constants> shadow_box_lines;

	std::optional<glm::mat4>shadow_frustum = std::nullopt;

	VkDescriptorSetLayout data_layout{};

	explicit debug_gfx() = default;
	~debug_gfx() = default;
	debug_gfx(const debug_gfx&) = delete;
	debug_gfx& operator=(const debug_gfx&) = delete;
	debug_gfx(debug_gfx&&) noexcept = default;
	debug_gfx& operator=(debug_gfx&&) noexcept = default;
	void init(const rh::ctx& ctx);
	void deinit(const rh::ctx& ctx) const;
	void set_shadow_frustum(debug_frustum frustum);
	void set_sunlight(glm::mat4 sunlight);
	[[nodiscard]] auto draw(mesh_ctx ctx, VkDeviceAddress scene_buffer_address) -> rh::result;
};