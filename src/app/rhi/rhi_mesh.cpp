#include "rhi.h"

void mesh_scene::init(const rh::ctx& ctx)
{
	const VkDevice device = ctx.rhi.device;
	std::vector<descriptor_allocator_growable::pool_size_ratio> frame_sizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
	};

	descriptor_allocator = descriptor_allocator_growable{};
	descriptor_allocator.value().init(device, 1000, frame_sizes);
}

void mesh_scene::deinit(const rh::ctx& ctx)
{
	const auto buffer = ctx.rhi.data.value();
	const VkDevice device = ctx.rhi.device;
	for (std::shared_ptr<mesh_asset> mesh : meshes)
	{
		gpu_mesh_buffers rectangle = mesh.get()->mesh_buffers;
		buffer->destroy_buffer(rectangle.vertex_buffer);
		buffer->destroy_buffer(rectangle.index_buffer);
		mesh.reset();
	}
	for (ktxVulkanTexture tx : ktx_vk_textures)
	{
		ktxVulkanTexture_Destruct(&tx, device, nullptr);
	}
	for (ktxTexture* tx : ktx_textures)
	{
		ktxTexture_Destroy(tx);
	}
	for (const VkImageView iv : image_views)
	{
		vkDestroyImageView(device, iv, nullptr);
	}
	if (descriptor_allocator.has_value()) {
		descriptor_allocator.value().destroy_pools(device);
	}
}

void mesh_scene::add_node(fastgltf::Node& gltf_node)
{
	const auto new_node = std::make_shared<mesh_node>();
	if (gltf_node.meshIndex.has_value())
	{
		new_node->mesh_index = gltf_node.meshIndex.value();

		auto [translation, rotation, scale] = std::get<fastgltf::TRS>(gltf_node.transform);

		const auto tr = glm::vec3(translation[0], translation[1], translation[2]);
		const auto ro = glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]);
		const auto sc = glm::vec3(scale[0], scale[1], scale[2]);

		const auto tm = translate(glm::mat4(1.0f), tr);
		const auto rm = toMat4(ro);
		const auto sm = glm::scale(glm::mat4(1.0f), sc);

		new_node->model_transform = tm * rm * sm;
	}
	new_node->children.reserve(gltf_node.children.size());
	for (size_t i : gltf_node.children)	new_node->children.emplace_back(i);
	nodes.push_back(new_node);
};

void mesh_scene::add_scene(fastgltf::Scene& gltf_scene)
{
	scenes.emplace_back(gltf_scene.nodeIndices.begin(), gltf_scene.nodeIndices.end());
};

[[nodiscard]] std::vector<render_object> mesh_scene::draw_queue(const size_t scene_index, const glm::mat4& m) const
{
	if (scene_index >= scenes.size()) return {};
	std::queue<std::shared_ptr<mesh_node>> queue{};
	std::vector<render_object> draw_nodes{};
	for (const size_t node_index : scenes[scene_index])
	{
		auto draw_node = nodes[node_index];
		draw_node->update(m, nodes);
		queue.push(draw_node);

	}
	while (queue.size() > 0)
	{
		const auto current_node = queue.front();
		queue.pop();
		if (current_node->mesh_index.has_value())
		{
			for (
				const std::shared_ptr<mesh_asset> ma = meshes[current_node->mesh_index.value()];
				const auto [start_index, count, material] : ma->surfaces)
			{
				render_object ro{};
				ro.transform = current_node->world_transform;
				ro.first_index = start_index;
				ro.index_count = count;
				ro.index_buffer = ma->mesh_buffers.index_buffer.buffer;
				ro.vertex_buffer_address = ma->mesh_buffers.vertex_buffer_address;
				draw_nodes.push_back(ro);
			}
		}
		for (const size_t child_index : current_node->children)
		{
			queue.push(nodes[child_index]);
		}
	}
	return draw_nodes;
};
