#include "Level.h"

#include <queue>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.inl>

using namespace rosy;

constexpr size_t max_node_stack_size = 4'096;
constexpr size_t max_stack_item_list = 16'384;

namespace
{
	constexpr auto gltf_to_ndc = glm::mat4(
		glm::vec4(-1.f, 0.f, 0.f, 0.f),
		glm::vec4(0.f, 1.f, 0.f, 0.f),
		glm::vec4(0.f, 0.f, 1.f, 0.f),
		glm::vec4(0.f, 0.f, 0.f, 1.f)
	);
}

result level::init(log* new_log, [[maybe_unused]] config new_cfg, camera* new_cam)
{
	l = new_log;
	cam = new_cam;
	return result::ok;
}

struct stack_item
{
	rosy_packager::node stack_node;
	glm::mat4 parent_transform{ glm::mat4{1.f}};
};

std::array<float, 16> mat4_to_array(glm::mat4 m)
{
	std::array<float, 16> rv{};
	const auto pos_r = glm::value_ptr(m);
	for (uint64_t i{ 0 }; i < 16; i++) rv[i] = pos_r[i];
	return rv;
}

result level::set_asset(const rosy_packager::asset& new_asset)
{
	const size_t root_scene_index = static_cast<size_t>(new_asset.root_scene);
	if (new_asset.scenes.size() <= root_scene_index) return result::invalid_argument;

	const auto& [nodes] = new_asset.scenes[root_scene_index];
	if (nodes.size() < 1) return result::invalid_argument;

	graphics_objects.clear();

	std::queue<stack_item> queue{};
	std::queue<uint32_t> mesh_queue{};
	for (const auto& node_index : nodes) {
		queue.push({
			.stack_node = new_asset.nodes[node_index],
			.parent_transform = glm::mat4{1.f},
		});
	}

	while (queue.size() > 0)
	{
		const stack_item current_stack_item = queue.front();
		queue.pop();

		glm::mat4 node_transform = glm::mat4(1.f);
		memcpy(glm::value_ptr(node_transform), current_stack_item.stack_node.transform.data(), current_stack_item.stack_node.transform.size());

		if (current_stack_item.stack_node.mesh_id < new_asset.meshes.size()) {
			mesh_queue.push(current_stack_item.stack_node.mesh_id);

			while (mesh_queue.size() > 0)
			{
				const auto current_mesh_index = mesh_queue.front();
				mesh_queue.pop();
				const rosy_packager::mesh current_mesh = new_asset.meshes[current_mesh_index];
				graphics_object go{};
				go.transform = mat4_to_array(gltf_to_ndc * current_stack_item.parent_transform * node_transform);
				go.surface_data.reserve(current_mesh.surfaces.size());
				size_t go_index = graphics_objects.size();
				for (const auto& current_surface : current_mesh.surfaces)
				{
					surface_graphics_data sgd{};
					sgd.graphics_object_index = go_index;
					sgd.material_index = current_surface.material;
					sgd.index_count = current_surface.count;
					sgd.start_index = current_surface.start_index;
					go.surface_data.push_back(sgd);
				}

				for (const uint32_t child_mesh_index : current_mesh.child_meshes)
				{
					mesh_queue.push(child_mesh_index);
				}
				graphics_objects.push_back(go);
			}
		}

		for (const size_t child_index : current_stack_item.stack_node.child_nodes)
		{
			queue.push({
				.stack_node = new_asset.nodes[child_index],
				.parent_transform = current_stack_item.parent_transform * node_transform,
			});
		}
	}

	return result::ok;
}


// ReSharper disable once CppMemberFunctionMayBeStatic
void level::deinit()
{
	// TODO: have things to deinit I guess.
}

