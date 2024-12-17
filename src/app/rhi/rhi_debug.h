#pragma once

struct debug_ctx
{
	std::vector<debug_draw_push_constants> lines{};
	glm::mat4 world_transform = { 1.f };
	size_t scene_index = 0;
	bool wire_frame = false;
	bool depth_enabled = true;
	VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE;
	VkCommandBuffer cmd;
	VkExtent2D extent;
	VkDescriptorSet* global_descriptor;
};

class debug_gfx
{
public:
	std::string name{ "debug" };
	float color[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
	std::string vertex_path{ "out/debug.vert.spv" };
	std::string frag_path{ "out/debug.frag.spv" };

	std::vector<VkDescriptorSetLayout> descriptor_layouts;

	std::optional<shader_pipeline> shaders = std::nullopt;
	std::optional<descriptor_allocator_growable> descriptor_allocator = std::nullopt;

	VkDescriptorSetLayout data_layout{};

	explicit debug_gfx() = default;
	~debug_gfx() = default;
	debug_gfx(const debug_gfx&) = delete;
	debug_gfx& operator=(const debug_gfx&) = delete;
	debug_gfx(debug_gfx&&) noexcept = default;
	debug_gfx& operator=(debug_gfx&&) noexcept = default;
	void init(const rh::ctx& ctx);
	void deinit(const rh::ctx& ctx);
	[[nodiscard]] auto draw(debug_ctx ctx) -> rh::result;
};