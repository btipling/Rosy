#include "Level.h"

#include <queue>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>

using namespace rosy;

constexpr size_t max_node_stack_size = 4'096;
constexpr size_t max_stack_item_list = 16'384;

namespace
{

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

	struct level_state
	{
		rosy::log* l{ nullptr };
		camera* cam{ nullptr };

		read_level_state* rls{ nullptr };
		write_level_state const* wls{ nullptr };

		rosy::result update() const
		{
			rls->p = cam->p;
			rls->v = cam->v;
			rls->vp = cam->vp;
			rls->cam_pos = cam->position;

			rls->debug_objects.clear();

			glm::mat4 debug_light{ 1.f };
			{
				// Calculate light data from wls
				const float sun_x = wls->sun_distance * glm::sin(wls->sun_rho) * glm::cos(wls->sun_theta);
				const float sun_y = wls->sun_distance * glm::sin(wls->sun_rho) * glm::sin(wls->sun_theta);
				const float sun_z = wls->sun_distance * glm::cos(wls->sun_rho);
				debug_light = glm::translate(debug_light, glm::vec3(sun_x, sun_y, sun_z));
			}

			debug_object line;
			line.type = debug_object_type::line;
			line.transform = mat4_to_array(glm::mat4(1.f));
			line.color = { 1.f, 0.f, 0.f, 1.f };
			rls->debug_objects.push_back(line);

			debug_object circle;
			circle.type = debug_object_type::circle;
			circle.transform = mat4_to_array(debug_light);
			circle.color = { 0.f, 0.f, 1.f, 1.f };
			rls->debug_objects.push_back(circle);

			return result::ok;
		}
	};

	level_state* ls{ nullptr };
	scene_graph_processor* sgp{ nullptr };

}

result level::init(log* new_log, [[maybe_unused]] config new_cfg, camera* new_cam)
{
	{
		// Init level state
		ls = new(std::nothrow) level_state;
		if (ls == nullptr)
		{
			new_log->error("level_state allocation failed");
			return result::allocation_failure;
		}
		ls->l = new_log;
		ls->cam = new_cam;
		ls->rls = &rls;
		ls->wls = &wls;
	}
	{
		// Init scene graph processor
		sgp = new(std::nothrow) scene_graph_processor;
		if (sgp == nullptr)
		{
			ls->l->error("scene_graph_processor allocation failed");
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
	if (ls)
	{
		delete ls;
		ls = nullptr;
	}
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
				const size_t go_index = graphics_objects.size();
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

	return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::update()
{
	return ls->update();
}
