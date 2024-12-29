#pragma once
#include "rhi_types.h"

class mesh_scene
{
public:
	std::string name{ "mesh" };
	float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	std::string vertex_path{ "out/mesh.spv" };
	std::string frag_path{ "out/mesh.spv" };
	std::string shadow_vertex_path{"out/shadow.spv"};
	std::string shadow_frag_path{ "out/shadow.spv" };

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
	[[nodiscard]] auto draw(mesh_ctx ctx) -> rh::result;
	[[nodiscard]] auto generate_shadows(mesh_ctx ctx) -> rh::result;
private:
	VkExtent2D shadow_map_extent_{};
	std::vector<render_object> draw_nodes_{};
};