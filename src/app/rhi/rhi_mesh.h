#pragma once
#include "rhi_types.h"
#include "../camera.h"

class camera;

struct shadow_map
{
	glm::mat4 view;
	glm::mat4 projection;
};

class mesh_scene
{
public:
	std::string name{ "mesh" };
	float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	std::string vertex_path{ "out/mesh.spv" };
	std::string frag_path{ "out/mesh.spv" };
	std::string shadow_vertex_path{"out/shadow.spv"};
	std::string shadow_frag_path{ "out/shadow.spv" };
	std::unique_ptr<camera> mesh_cam = nullptr;

	bool depth_bias_enabled{ false };
	float depth_bias_constant{ 0.f };
	float depth_bias_clamp{ 0.f };
	float depth_bias_slope_factor{ 0.f };

	size_t root_scene = 0;
	std::vector<std::vector<size_t>>scenes;
	std::vector<std::shared_ptr<mesh_node>> nodes;
	std::vector<std::shared_ptr<mesh_asset>> meshes;
	std::optional<gpu_render_buffers> render_buffers;
	std::optional<gpu_material_buffers> material_buffers;
	std::optional<gpu_scene_buffers> scene_buffers;
	std::vector<material> materials;

	std::vector<ktx_auto_texture> ktx_textures;
	std::vector<VkSampler> samplers;
	std::vector<VkImageView> image_views;

	std::optional<shader_pipeline> shaders;
	std::optional<shader_pipeline> shadow_shaders;

	VkDescriptorSetLayout image_layout{};

	std::shared_ptr<debug_gfx> debug{};

	explicit mesh_scene() = default;
	~mesh_scene() = default;
	mesh_scene(const mesh_scene&) = delete;
	mesh_scene& operator=(const mesh_scene&) = delete;
	mesh_scene(mesh_scene&&) noexcept = default;
	mesh_scene& operator=(mesh_scene&&) noexcept = default;
	void init(const rh::ctx& ctx);
	void init_shadows(const rh::ctx& ctx);
	void update(mesh_ctx ctx, std::optional<gpu_scene_data> scene_data = std::nullopt);
	void deinit(const rh::ctx& ctx) const;
	void add_node(fastgltf::Node& gltf_node);
	void add_scene(fastgltf::Scene& gltf_scene);
	[[nodiscard]] glm::mat4 sunlight(const rh::ctx& ctx);
	[[nodiscard]] auto csm_pos(const rh::ctx& ctx) -> glm::mat4;
	void draw_ui(const rh::ctx& ctx);
	[[nodiscard]] auto draw(mesh_ctx ctx) -> rh::result;
	[[nodiscard]] auto generate_shadows(mesh_ctx ctx, int pass_number) -> rh::result;
	std::vector<glm::vec4> shadow_map_frustum(const glm::mat4& proj, const glm::mat4& view);
	glm::mat4 shadow_map_view(const std::vector<glm::vec4>& shadow_frustum, const glm::vec3 light_direction);
	shadow_map shadow_map_projection(const std::vector<glm::vec4>& shadow_frustum, const glm::mat4& shadow_map_view);
	shadow_map shadow_map_projection(const rh::ctx& ctx, const glm::vec3 light_direction, const glm::mat4& p, const glm::mat4& world_view);
private:
	glm::mat4 light_transform_ = glm::identity<glm::mat4>();
	float sunlight_x_rot_{ 0.f };
	float sunlight_y_rot_{ 0.f };
	float sunlight_z_rot_{ 0.f };
	glm::vec4 q0_;
	glm::vec4 q1_;
	glm::vec4 q2_;
	glm::vec4 q3_;
	glm::vec4 q4_;
	glm::vec4 q5_;
	glm::vec4 q6_;
	glm::vec4 q7_;
	float max_x_;
	float min_x_;
	float max_y_;
	float min_y_;
	float max_z_;
	float min_z_;

	glm::vec3 light_pos_{ 1.f };
	float cascade_factor_{ 0.5 };
	VkExtent2D shadow_map_extent_{};
	std::vector<render_object> draw_nodes_{};
};