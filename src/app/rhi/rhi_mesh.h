#pragma once

class mesh_scene
{
public:
	size_t root_scene = 0;
	std::vector<std::vector<size_t>>scenes;
	std::vector<std::shared_ptr<mesh_node>> nodes;
	std::vector<std::shared_ptr<mesh_asset>> meshes;
	std::vector<material> materials;

	std::vector<ktxVulkanTexture> ktx_vk_textures;
	std::vector<ktxTexture*> ktx_textures;
	std::vector<VkSampler> samplers;
	std::vector<VkImageView> image_views;
	std::vector<VkDescriptorSet> descriptor_sets;
	std::vector<VkDescriptorSetLayout> descriptor_layouts;

	std::optional<shader_pipeline> shaders;
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
	void deinit(const rh::ctx& ctx);
	void add_node(fastgltf::Node& gltf_node);
	void add_scene(fastgltf::Scene& gltf_scene);
	[[nodiscard]] std::vector<render_object> draw_queue(const size_t scene_index, const glm::mat4& m = { 1.f }) const;
};