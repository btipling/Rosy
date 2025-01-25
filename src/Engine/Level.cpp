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

	struct stack_item
	{
		rosy_packager::node stack_node;
		glm::mat4 parent_transform{ glm::mat4{1.f} };
	};

	struct scene_graph_processor
	{
		std::queue<stack_item> queue{};
		std::queue<uint32_t> mesh_queue{};
	};


	scene_graph_processor* sgp{ nullptr };

}

result level::init(log* new_log, [[maybe_unused]] config new_cfg, camera* new_cam)
{
	l = new_log;
	cam = new_cam;
	{
		// Init scene graph processor
		sgp = new(std::nothrow) scene_graph_processor;
		if (sgp == nullptr)
		{
			l->error("scene_graph_processor allocation failed");
			return result::allocation_failure;
		}
	}
	return result::ok;
}


// ReSharper disable once CppMemberFunctionMayBeStatic
void level::deinit()
{
	if (sgp)
	{
		delete sgp;
		sgp = nullptr;
	}
}

std::array<float, 16> mat4_to_array(glm::mat4 m)
{
	std::array<float, 16> a{};
	const auto pos_r = glm::value_ptr(m);
	for (uint64_t i{ 0 }; i < 16; i++) a[i] = pos_r[i];
	return a;
}

glm::mat4 array_to_mat4(std::array<float, 16> a)
{
	glm::mat4 m{};
	const auto pos_r = glm::value_ptr(m);
	for (uint64_t i{ 0 }; i < 16; i++) pos_r[i] = a[i];
	return m;
}

result level::set_asset(const rosy_packager::asset& new_asset)
{
	const size_t root_scene_index = static_cast<size_t>(new_asset.root_scene);
	if (new_asset.scenes.size() <= root_scene_index) return result::invalid_argument;

	const auto& [nodes] = new_asset.scenes[root_scene_index];
	if (nodes.size() < 1) return result::invalid_argument;

	// Reset state
	graphics_objects.clear();
	while (!sgp->queue.empty()) sgp->queue.pop();
	while (!sgp->mesh_queue.empty()) sgp->mesh_queue.pop();

	for (const auto& node_index : nodes) {
		sgp->queue.push({
			.stack_node = new_asset.nodes[node_index],
			.parent_transform = glm::mat4{1.f},
		});
	}

	while (sgp->queue.size() > 0)
	{
		// ReSharper disable once CppUseStructuredBinding Visual studio wants to make this a reference, and it shouldn't be.
		const stack_item queue_item = sgp->queue.front();
		sgp->queue.pop();

		glm::mat4 node_transform = array_to_mat4(queue_item.stack_node.transform);

		if (queue_item.stack_node.mesh_id < new_asset.meshes.size()) {
			sgp->mesh_queue.push(queue_item.stack_node.mesh_id);

			while (sgp->mesh_queue.size() > 0)
			{
				const auto current_mesh_index = sgp->mesh_queue.front();
				sgp->mesh_queue.pop();
				const rosy_packager::mesh current_mesh = new_asset.meshes[current_mesh_index];
				graphics_object go{};
				go.transform = mat4_to_array(gltf_to_ndc * queue_item.parent_transform * node_transform);
				go.surface_data.reserve(current_mesh.surfaces.size());
				size_t go_index = graphics_objects.size();
				for (const auto& [sur_start_index, sur_count, sur_material] : current_mesh.surfaces)
				{
					surface_graphics_data sgd{};
					sgd.mesh_index = current_mesh_index;
					sgd.graphics_object_index = go_index;
					sgd.material_index = sur_material;
					sgd.index_count = sur_count;
					sgd.start_index = sur_start_index;
					go.surface_data.push_back(sgd);
				}

				for (const uint32_t child_mesh_index : current_mesh.child_meshes)
				{
					sgp->mesh_queue.push(child_mesh_index);
				}
				graphics_objects.push_back(go);
			}
		}

		for (const size_t child_index : queue_item.stack_node.child_nodes)
		{
			sgp->queue.push({
				.stack_node = new_asset.nodes[child_index],
				.parent_transform = queue_item.parent_transform * node_transform,
			});
		}
	}

	wls.light[0] = 0.f;
	wls.light[1] = 1.f;
	wls.light[2] = 2.f;

	return result::ok;
}

result level::update()
{
	rls.p = cam->p;
	rls.v = cam->v;
	rls.vp = cam->vp;
	rls.cam_pos = cam->position;
	return result::ok;
}
