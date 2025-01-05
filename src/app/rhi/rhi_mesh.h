#pragma once
#include "rhi_types.h"
#include "../camera.h"

enum class camera_view;
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
	std::string shadow_vertex_path{ "out/shadow.spv" };
	std::string shadow_frag_path{ "out/shadow.spv" };
	std::unique_ptr<camera> mesh_cam = nullptr;

	bool depth_bias_enabled{ true };
	float depth_bias_constant{ -9.f };
	float depth_bias_clamp{ 0.f };
	float depth_bias_slope_factor{ -4.5f };

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

	// TODO: (skybox-fix) make the following private
	gpu_scene_data scene_data = {};
	glm::vec3 scene_rot = glm::vec3(0.f);
	glm::vec3 scene_pos = glm::vec3(0.f, 2.5f, 2.5f);
	float scene_scale = 0.1f;
	glm::mat4 shadow_map_view_old{ 1.f };
	bool toggle_wire_frame{ false };

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
	[[nodiscard]] auto csm_pos(int csm_extent) -> glm::mat4;
	void draw_ui(const rh::ctx& ctx);
	[[nodiscard]] auto draw(mesh_ctx ctx) -> rh::result;
	[[nodiscard]] auto generate_shadows(mesh_ctx ctx, int pass_number) -> rh::result;
	std::vector<glm::vec4> shadow_map_frustum(const glm::mat4& proj, const glm::mat4& view);
	glm::mat4 shadow_map_view(const std::vector<glm::vec4>& shadow_frustum, const glm::vec3 light_direction);
	shadow_map shadow_map_projection(const std::vector<glm::vec4>& shadow_frustum, const glm::mat4& shadow_map_view);
	shadow_map shadow_map_projection(const rh::ctx& ctx, const glm::vec3 light_direction, const glm::mat4& p, const glm::mat4& world_view);

	gpu_scene_data scene_update(const rh::ctx&);
private:
	float csm_dk_{ 0.f };
	int csm_extent_{ 0 };

	glm::mat4 light_transform_{ glm::identity<glm::mat4>() };
	glm::vec3 light_pos_{ 1.f };
	camera_view current_view_{ camera_view::camera };
	float sunlight_x_rot_{ 4.5f };
	float sunlight_y_rot_{ 0.f };
	float sunlight_z_rot_{ 0.f };

	float cascade_factor_{ 0.111f };
	VkExtent2D shadow_map_extent_{};
	std::vector<render_object> draw_nodes_{};
};