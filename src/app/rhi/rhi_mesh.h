#pragma once

struct mesh_ctx
{
	glm::mat4 world_transform = { 1.f };
	size_t scene_index = 0;
	bool wire_frame = false;
	bool depth_enabled = true;
	VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE;
	VkCommandBuffer cmd;
	VkExtent2D extent;
	VkDescriptorSet* global_descriptor;
};

class mesh_scene
{
public:
	const char* name = "mesh";
	float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	const char* vertex_path = "out/mesh.vert.spv";
	const char* frag_path = "out/mesh.frag.spv";
	const char* shadow_vertex_path = "out/shadow.vert.spv";
	const char* shadow_frag_path = "out/shadow.frag.spv";

	size_t root_scene = 0;
	std::vector<std::vector<size_t>>scenes;
	std::vector<std::shared_ptr<mesh_node>> nodes;
	std::vector<std::shared_ptr<mesh_asset>> meshes;
	std::vector<material> materials;

	std::vector<ktx_auto_texture> ktx_textures;
	std::vector<VkSampler> samplers;
	std::vector<VkImageView> image_views;
	std::vector<VkDescriptorSet> descriptor_sets;
	std::vector<VkDescriptorSetLayout> descriptor_layouts;
	std::vector<VkDescriptorSetLayout> shadow_descriptor_layouts;

	std::optional<shader_pipeline> shaders;
	std::optional<shader_pipeline> shadow_shaders;
	std::optional<descriptor_allocator_growable> descriptor_allocator = std::nullopt;

	VkDescriptorSetLayout data_layout{};
	VkDescriptorSetLayout image_layout{};

	explicit mesh_scene() = default;
	~mesh_scene() = default;
	mesh_scene(const mesh_scene&) = delete;
	mesh_scene& operator=(const mesh_scene&) = delete;
	mesh_scene(mesh_scene&&) noexcept = default;
	mesh_scene& operator=(mesh_scene&&) noexcept = default;
	void init(const rh::ctx& ctx);
	void init_shadows(const rh::ctx& ctx);
	void deinit(const rh::ctx& ctx);
	void add_node(fastgltf::Node& gltf_node);
	void add_scene(fastgltf::Scene& gltf_scene);
	[[nodiscard]] auto draw(mesh_ctx ctx) -> rh::result;
	[[nodiscard]] auto generate_shadows(mesh_ctx ctx) -> rh::result;
private:
	VkExtent2D shadow_map_extent_{};
	[[nodiscard]] std::vector<render_object> draw_queue(const size_t scene_index, const glm::mat4& m = { 1.f }) const;
};