#include "Level.h"
#include "Node.h"
#include <print>
#include <queue>
#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/quaternion.hpp>
#pragma warning(disable: 4127)
#include <flecs.h>
#pragma warning(default: 4127)
#include <numbers>

using namespace rosy;




constexpr size_t max_node_stack_size = 4'096;
constexpr size_t max_stack_item_list = 16'384;

const std::string mobs_node_name{ "mobs" };

namespace
{
	/**** ECS DEFINITIONS ****/

	// Components
	struct c_position
	{
		[[maybe_unused]] float x{ 0.f };
		[[maybe_unused]] float y{ 0.f };
		[[maybe_unused]] float z{ 0.f };
	};

	ECS_COMPONENT_DECLARE(c_position);
	struct c_mob
	{
		size_t index{ 0 };
	};
	ECS_COMPONENT_DECLARE(c_mob);

	// Tags
	struct [[maybe_unused]] t_rosy {};
	ECS_TAG_DECLARE(t_rosy);
	struct [[maybe_unused]] t_floor {};
	ECS_TAG_DECLARE(t_floor);

	// System definitions are forward declared and defined at the end.
	void detect_mob(ecs_iter_t* it);
	void detect_floor(ecs_iter_t* it);

	/**** LEVEL STATE DEFINITIONS ****/

	std::array<float, 16> mat4_to_array(glm::mat4 m)
	{
		std::array<float, 16> a{};
		const auto pos_r = glm::value_ptr(m);
		for (uint64_t i{ 0 }; i < 16; i++) a[i] = pos_r[i];
		return a;
	}

	glm::mat4 array_to_mat4(const std::array<float, 16>& a)
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
		node* game_node{ nullptr };
		rosy_packager::node stack_node;
		glm::mat4 parent_transform{ glm::mat4{1.f} };
		bool is_mob{ false };
	};

	struct scene_graph_processor
	{
		std::queue<stack_item> queue{};
		std::queue<uint32_t> mesh_queue{};
	};

	// Game nodes are double referenced by entity id and index in a vector.
	struct game_node_reference
	{
		ecs_entity_t entity{ 0 };
		[[maybe_unused]] size_t index{ 0 };
		node* node{ nullptr };
	};

	struct level_state
	{
		rosy::log* l{ nullptr };
		camera* cam{ nullptr };

		read_level_state* rls{ nullptr };
		write_level_state const* wls{ nullptr };
		node* level_game_node{ nullptr };
		// Important game nodes that can be referenced via their graphics object index
		std::vector<game_node_reference> game_nodes;

		// ECS
		ecs_world_t* world{ nullptr };
		ecs_entity_t level{};

		rosy::result init()
		{
			level_game_node = new(std::nothrow) node;
			if (level_game_node == nullptr)
			{
				l->error("root scene_objects allocation failed");
				return result::allocation_failure;
			};
			if (const auto res = level_game_node->init(l, {}, {}); res != result::ok)
			{
				l->error("root scene_objects initialization failed");
				delete level_game_node;
				level_game_node = nullptr;
				return result::error;
			}
			level_game_node->name = "rosy_root";

			world = ecs_init();
			if (world == nullptr)
			{
				l->error("flecs world init failed");
				return result::error;
			}
			level = ecs_new(world);
			assert(ecs_is_alive(world, level));

			init_components();
			init_tags();
			init_systems();

			return result::ok;
		}

		void deinit()
		{
			for (const game_node_reference gnr : game_nodes)
			{
				ecs_delete(world, gnr.entity);
			}
			if (level_game_node != nullptr) {
				level_game_node->deinit();
				delete level_game_node;
				level_game_node = nullptr;
			}
			if (ecs_is_alive(world, level))
			{
				ecs_delete(world, level);
				assert(!ecs_is_alive(world, level));
			}
			if (world != nullptr)
			{
				ecs_fini(world);
			}
		}

		void init_components() const
		{
			ECS_COMPONENT_DEFINE(world, c_position);
			ECS_COMPONENT_DEFINE(world, c_mob);
		}

		void init_tags() const
		{
			ECS_TAG_DEFINE(world, t_rosy);
			ECS_TAG_DEFINE(world, t_floor);
		}

		void init_systems()
		{

			{
				ecs_system_desc_t desc{};
				{
					ecs_entity_desc_t e_desc{};
					e_desc.id = 0;
					e_desc.name = "detect_mob";
					{
						ecs_id_t add_ids[3];
						add_ids[0] = { ecs_dependson(EcsOnUpdate) };
						add_ids[1] = EcsOnUpdate;
						add_ids[2] = 0;
						e_desc.add = add_ids;
					}
					desc.entity = ecs_entity_init(world, &e_desc);
				}
				desc.query.expr = "c_mob";
				desc.ctx = static_cast<void*>(this);
				desc.callback = detect_mob;
				ecs_system_init(world, &desc);
			}
			{
				ecs_system_desc_t desc{};
				{
					ecs_entity_desc_t e_desc{};
					e_desc.id = 0;
					e_desc.name = "detect_floor";
					{
						ecs_id_t add_ids[3];
						add_ids[0] = { ecs_dependson(EcsOnUpdate) };
						add_ids[1] = EcsOnUpdate;
						add_ids[2] = 0;
						e_desc.add = add_ids;
					}
					desc.entity = ecs_entity_init(world, &e_desc);
				}
				desc.query.expr = "t_floor";
				desc.ctx = static_cast<void*>(this);
				desc.callback = detect_floor;
				ecs_system_init(world, &desc);
			}
		}

		[[nodiscard]] std::vector<node*> get_mobs() const
		{
			if (level_game_node == nullptr) return {};
			if (level_game_node->children.empty()) return {};
			for (const node* root_node = level_game_node->children[0]; node * child : root_node->children)
			{
				if (child->name == "mobs")
				{
					return child->children;
				}
			}
			return {};
		}

		[[nodiscard]] std::vector<node*> get_static() const
		{
			if (level_game_node == nullptr) return {};
			if (level_game_node->children.empty()) return {};
			for (const node* root_node = level_game_node->children[0]; node * child : root_node->children)
			{
				if (child->name == "static")
				{
					return child->children;
				}
			}
			return {};
		}

		rosy::result update(bool* updated, uint64_t delta_time) const
		{
			{
				// Configure initial camera
				rls->cam.p = cam->p;
				rls->cam.v = cam->v;
				rls->cam.vp = cam->vp;
				rls->cam.position = cam->position;
				rls->cam.pitch = cam->pitch;
				rls->cam.yaw = cam->yaw;
			}
			{
				// Configure draw options based on writable level state
				rls->debug_enabled = wls->enable_edit;
				rls->draw_config = wls->draw_config;
				rls->light = wls->light;
			}
			{
				// Fragment config
				rls->fragment_config = wls->fragment_config;
			}
			{
				// Mob state
				std::vector<node*> mobs = get_mobs();
				if (wls->mob_edit.submitted)
				{
					rls->mob_read.clear_edits = true;
					if (mobs.size() > wls->mob_edit.edit_index)
					{
						if (const auto res = mobs[0]->set_position(wls->mob_edit.position); res != result::ok)
						{
							l->error("Error updating mob position");
						}
						else
						{
							*updated = true;
						}

					}
				}
				else
				{
					rls->mob_read.clear_edits = false;
				}
				rls->mob_read.mob_states.clear();
				for (const node* const n : mobs)
				{
					rls->mob_read.mob_states.push_back({
						.name = n->name,
						.position = {n->position[0], n->position[1], n->position[2]},
						});
				}
			}

			rls->debug_objects.clear();
			{
				// Light & Shadow logic
				glm::mat4 light_sun_view;
				glm::mat4 debug_light_sun_view;
				glm::mat4 debug_light_translate;
				glm::mat4 light_line_rot;
				{
					// Lighting math

					{
						const glm::mat4 light_translate = glm::translate(glm::mat4(1.f), { 0.f, 0.f, 1.f * wls->light_debug.sun_distance });
						debug_light_translate = glm::translate(glm::mat4(1.f), { 0.f, 0.f, -1.f * wls->light_debug.sun_distance });
						const glm::quat pitch_rotation = angleAxis(-wls->light_debug.sun_pitch, glm::vec3{ 1.f, 0.f, 0.f });
						const glm::quat yaw_rotation = angleAxis(wls->light_debug.sun_yaw, glm::vec3{ 0.f, -1.f, 0.f });
						light_line_rot = toMat4(yaw_rotation) * toMat4(pitch_rotation);

						const auto camera_position = glm::vec3(light_line_rot * glm::vec4(0.f, 0.f, -wls->light_debug.sun_distance, 0.f));
						auto sunlight = glm::vec4(glm::normalize(camera_position), 1.f);
						light_sun_view = light_line_rot * light_translate;
						debug_light_sun_view = light_line_rot * (wls->light_debug.enable_light_perspective ? light_translate : debug_light_translate);

						rls->light.sunlight = { sunlight[0], sunlight[1], sunlight[2], sunlight[3] };
					};
				}

				if (wls->light_debug.enable_sun_debug) {
					// Generate debug lines for light and shadow debugging
					const glm::mat4 debug_draw_view = light_line_rot * debug_light_translate;
					const glm::mat4 debug_light_line = glm::scale(debug_draw_view, { wls->light_debug.sun_distance, wls->light_debug.sun_distance, wls->light_debug.sun_distance });

					debug_object line;
					line.type = debug_object_type::line;
					line.transform = mat4_to_array(debug_light_line);
					line.color = { 1.f, 0.f, 0.f, 1.f };
					if (wls->light_debug.enable_sun_debug) rls->debug_objects.push_back(line);
					{
						// Two circles to represent a sun
						constexpr float angle_step{ glm::pi<float>() / 4.f };
						for (size_t i{ 0 }; i < 4; i++) {
							debug_object sun_circle;
							glm::mat4 m{ 1.f };
							m = glm::rotate(m, angle_step * static_cast<float>(i), { 1.f, 0.f, 0.f });
							sun_circle.type = debug_object_type::circle;
							sun_circle.transform = mat4_to_array(debug_draw_view * m);
							sun_circle.color = { 0.976f, 0.912f, 0.609f, 1.f };
							rls->debug_objects.push_back(sun_circle);
						}
					}
				}

				glm::mat4 cam_lv;
				glm::mat4 cam_lp;
				{
					// Create Light view and projection

					const float cascade_level = wls->light_debug.cascade_level;
					auto light_projections = glm::mat4(
						glm::vec4(2.f / cascade_level, 0.f, 0.f, 0.f),
						glm::vec4(0.f, -2.f / cascade_level, 0.f, 0.f),
						glm::vec4(0.f, 0.f, -1.f / wls->light_debug.orthographic_depth, 0.f),
						glm::vec4(0.f, 0.f, 0.f, 1.f)
					);

					const glm::mat4 lv = light_sun_view;
					const glm::mat4 lp = light_projections;
					cam_lv = glm::inverse(debug_light_sun_view);
					cam_lp = wls->light_debug.enable_light_perspective ? light_projections : array_to_mat4((rls->cam.p));
					rls->cam.shadow_projection_near = mat4_to_array(lp * glm::inverse(lv));
				}

				if (wls->light_debug.enable_light_cam)
				{
					// Set debug lighting options on
					rls->debug_enabled = false;
					rls->cam.v = mat4_to_array(cam_lv);
					rls->cam.vp = mat4_to_array(cam_lp * cam_lv);
				}
			}
			ecs_progress(world, static_cast<float>(delta_time));
			return result::ok;
		}
	};

	level_state* ls{ nullptr };
	scene_graph_processor* sgp{ nullptr };

	// **** ECS SYSTEM DEFINITIONS ****/

	// TODO: add level state context to world.
	// ReSharper disable once CppParameterMayBeConstPtrOrRef
	void detect_mob(ecs_iter_t* it)
	{
		const auto ctx = static_cast<level_state*>(it->param);
		const auto p = ecs_field(it, c_mob, 0);

		for (int i = 0; i < it->count; i++) {
			ecs_entity_t e = it->entities[i];

			const game_node_reference ref = ctx->game_nodes[p[i].index];
			ctx->l->info(std::format("mob detected @ index {} with name: '{}'", static_cast<int>(p[i].index), ref.node->name));
			if (ecs_has_id(ctx->world, e, ecs_id(t_rosy)))
			{
				ctx->l->info(std::format("Rosy detected!"));
			}
		}
	}

	// ReSharper disable once CppParameterMayBeConstPtrOrRef
	void detect_floor(ecs_iter_t* it)
	{
		const auto ctx = static_cast<level_state*>(it->param);

		for (int i = 0; i < it->count; i++) {
			ctx->l->info(std::format("floor detected"));
		}
	}

}

result level::init(log* new_log, [[maybe_unused]] const config new_cfg)
{
	{
		// Init level state
		{
			wls.light_debug.sun_distance = 23.776f;
			wls.light_debug.sun_pitch = 5.849f;
			wls.light_debug.sun_yaw = 6.293f;
			wls.light_debug.orthographic_depth = 49.060f;
			wls.light_debug.cascade_level = 29.194f;
			wls.light.depth_bias_constant = -21.882f;
			wls.light.depth_bias_clamp = -20.937f;
			wls.light.depth_bias_slope_factor = -163.064f;
			wls.draw_config.cull_enabled = true;
			wls.light.depth_bias_enabled = true;
			wls.draw_config.thick_wire_lines = false;
			wls.fragment_config.output = 0;
			wls.fragment_config.light_enabled = true;
			wls.fragment_config.tangent_space_enabled = true;
			wls.fragment_config.shadows_enabled = true;
		}
		ls = new(std::nothrow) level_state;
		if (ls == nullptr)
		{
			new_log->error("level_state allocation failed");
			return result::allocation_failure;
		}
		ls->l = new_log;
		if (const auto res = ls->init(); res != result::ok)
		{
			new_log->error("level_state init failed");
			return res;
		}

		// Camera initialization
		{
			cam = new(std::nothrow) camera{};
			cam->starting_x = -5.88f;
			cam->starting_y = 3.86f;
			cam->starting_z = -1.13f;
			cam->starting_pitch = 0.65f;
			cam->starting_yaw = -1.58f;
			if (cam == nullptr)
			{
				ls->l->error("Error allocating camera");
				return result::allocation_failure;
			}
			if (auto const res = cam->init(ls->l, new_cfg); res != result::ok)
			{
				ls->l->error(std::format("Camera creation failed: {}", static_cast<uint8_t>(res)));
				return res;
			}
		}
		ls->cam = cam;
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
	if (cam)
	{
		cam->deinit();
		delete cam;
		cam = nullptr;
	}

	if (sgp)
	{
		delete sgp;
		sgp = nullptr;
	}
	if (ls)
	{
		ls->deinit();
		delete ls;
		ls = nullptr;
	}
}

result level::set_asset(const rosy_packager::asset& new_asset)
{
	// Traverse the assets to construct the scene graph and track important game play entities.

	const size_t root_scene_index = static_cast<size_t>(new_asset.root_scene);
	if (new_asset.scenes.size() <= root_scene_index) return result::invalid_argument;

	const auto& [nodes] = new_asset.scenes[root_scene_index];
	if (nodes.size() < 1) return result::invalid_argument;

	{
		// Clear existing game nodes
		for (node* n : ls->level_game_node->children) n->deinit();
		ls->level_game_node->children.clear();
	}
	{
		// Reset state
		while (!sgp->queue.empty()) sgp->queue.pop();
		while (!sgp->mesh_queue.empty()) sgp->mesh_queue.pop();
	}

	// Prepopulate the node queue with the root scenes nodes
	for (const auto& node_index : nodes) {
		const rosy_packager::node new_node = new_asset.nodes[node_index];

		// Game nodes are a game play representation of a graphics object, and can be static or a mob.
		auto new_game_node = new(std::nothrow) node;
		if (new_game_node == nullptr)
		{
			ls->l->error("initial scene_objects allocation failed");
			return result::allocation_failure;
		}

		// The initial parent transform to populate the scene graph hierarchy is an identity matrix.
		const std::array<float, 16> identity_m = mat4_to_array(glm::mat4(1.f));

		// All nodes must be initialized here and below.
		if (const auto res = new_game_node->init(ls->l, new_node.transform, identity_m); res != result::ok)
		{
			ls->l->error("initial scene_objects initialization failed");
			new_game_node->deinit(); delete new_game_node;
			delete new_game_node;
			return result::error;
		}

		// All nodes have a name.
		new_game_node->name = std::string(new_node.name.begin(), new_node.name.end());

		// Root nodes are directly owned by the level state.
		ls->level_game_node->children.push_back(new_game_node);

		// Populate the node queue.
		sgp->queue.push({
			.game_node = new_game_node,
			.stack_node = new_node,
			.parent_transform = glm::mat4{1.f},
			.is_mob = false, // Root nodes are assumed to be static.
			});
	}

	std::vector<graphics_object> mob_graphics_objects;

	// Use two indices to track either static graphics objects or dynamic "mobs"
	size_t go_mob_index{ 0 };
	size_t go_static_index{ 0 };
	while (sgp->queue.size() > 0)
	{
		// ReSharper disable once CppUseStructuredBinding Visual studio wants to make this a reference, and it shouldn't be.
		stack_item queue_item = sgp->queue.front();
		sgp->queue.pop();
		assert(queue_item.game_node != nullptr);

		glm::mat4 node_transform = array_to_mat4(queue_item.stack_node.transform);
		const glm::mat4 transform = gltf_to_ndc * queue_item.parent_transform * node_transform;

		// Advance the next item in the node queue
		if (queue_item.stack_node.mesh_id < new_asset.meshes.size()) {
			// Each node has a mesh id. Add to mesh queue.
			sgp->mesh_queue.push(queue_item.stack_node.mesh_id);

			//Advance the next item in the mesh queue. 
			while (sgp->mesh_queue.size() > 0)
			{
				// Get mesh index for current mesh in queue and remove from queue.
				const auto current_mesh_index = sgp->mesh_queue.front();
				sgp->mesh_queue.pop();

				// Get the mesh from the asset using the mesh index
				const rosy_packager::mesh current_mesh = new_asset.meshes[current_mesh_index];

				// Declare a new graphics object.
				graphics_object go{};

				{
					// There are two separate sets of indices to track, one for static graphics objects and one for dynamic "mobs"
					go.index = queue_item.is_mob ? go_mob_index : go_static_index;
				}

				{
					// Record the assets transforms from the asset
					const glm::mat4 object_space_transform = glm::inverse(static_cast<glm::mat3>(transform));
					const glm::mat4 normal_transform = glm::transpose(object_space_transform);
					go.transform = mat4_to_array(transform);
					go.normal_transform = mat4_to_array(normal_transform);
					go.object_space_transform = mat4_to_array(object_space_transform);
				}

				{
					// Each mesh has any number of surfaces that are derived from gltf primitives for example. These are what are given to the renderer to draw.
					go.surface_data.reserve(current_mesh.surfaces.size());
					for (const auto& [sur_start_index, sur_count, sur_material] : current_mesh.surfaces)
					{
						surface_graphics_data sgd{};
						sgd.mesh_index = current_mesh_index;
						// The index written to the surface graphic object is critical for pulling its transforms out of the buffer for rendering.
						// The two separate indices indicate where renderer's buffer on the GPU its data will live. An offset is used for
						// mobs that is equal to the total number of static surface graphic objects as mobs are all at the end of the buffer.
						// The go_mob_index is also used to identify individual mobs for game play purposes.
						sgd.graphics_object_index = queue_item.is_mob ? go_mob_index : go_static_index;
						sgd.material_index = sur_material;
						sgd.index_count = sur_count;
						sgd.start_index = sur_start_index;
						if (new_asset.materials.size() > sur_material && new_asset.materials[sur_material].alpha_mode != 0) {
							sgd.blended = true;
						}
						go.surface_data.push_back(sgd);
					}
				}

				{
					// Each mesh may have an arbitrary number of child meshes. Add them to the mesh queue.
					for (const uint32_t child_mesh_index : current_mesh.child_meshes)
					{
						sgp->mesh_queue.push(child_mesh_index);
					}
				}
				{
					// Dynamic "mobs" are put at the end of the buffer in the renderer so they can be updated dynamically without having to update
					// the entire graphics object buffer. Here is where they are put into one of the two buckets, either static which go in the front of the buffer
					// or mob which go at the end.
					if (queue_item.is_mob)
					{
						mob_graphics_objects.push_back(go);
						go_mob_index += 1;
					}
					else
					{
						graphics_objects.push_back(go);
						go_static_index += 1;
					}
					// Also track all the graphic objects in a combined bucket.
					queue_item.game_node->graphics_objects.push_back(go);
				}
			}
		}

		// Each node can have an arbitrary number of child nodes.
		for (const size_t child_index : queue_item.stack_node.child_nodes)
		{
			// This is the same node initialization sequence from the root's scenes logic above.
			const rosy_packager::node new_node = new_asset.nodes[child_index];

			// Create the pointer
			auto new_game_node = new(std::nothrow) node;
			if (new_game_node == nullptr)
			{
				ls->l->error("Error allocating new game node in set asset");
				return result::allocation_failure;
			}

			// Initialize its state
			if (const auto res = new_game_node->init(ls->l, mat4_to_array(transform), mat4_to_array(node_transform)); res != result::ok)
			{
				ls->l->error("Error initializing new game node in set asset");
				new_game_node->deinit();
				delete new_game_node;
				return result::error;
			}

			// Give it a name
			new_game_node->name = std::string(new_node.name.begin(), new_node.name.end());

			// New nodes are recorded as children of their parent to form a scene graph.
			queue_item.game_node->children.push_back(new_game_node);

			// Mobs are a child of "mob" node or are ancestors of the "mob" node's children
			const bool is_mob = queue_item.is_mob || new_game_node->name == mobs_node_name;

			// Add the node to the node queue to have their meshes and primitives processed.
			sgp->queue.push({
				.game_node = new_game_node,
				.stack_node = new_node,
				.parent_transform = queue_item.parent_transform * node_transform,
				.is_mob = is_mob,
				});
		}
	}

	{
		// Record the indices of static and mob graphics objects for rendering and other uses as explained above.
		rls.graphic_objects.static_objects_offset = go_static_index;
		static_objects_offset = go_static_index;
		num_dynamic_objects = go_mob_index;
	}
	{
		// It is at this point that the total number of static graphic object is known. Traverse all the mobs graphic object to record that offset.
		for (size_t i{ 0 }; i < mob_graphics_objects.size(); i++)
		{
			for (size_t j{ 0 }; j < mob_graphics_objects[i].surface_data.size(); j++)
			{
				mob_graphics_objects[i].surface_data[j].graphic_objects_offset = static_objects_offset;
			}
		}
	}
	{
		// Merge the static objects and mob graphic objects into one vector to be sent to the renderer for the initial upload to the gpu
		graphics_objects.insert(graphics_objects.end(), mob_graphics_objects.begin(), mob_graphics_objects.end());
	}
	{
		// Print the result of all this if debug logging is on.
		ls->level_game_node->debug();
	}
	{
		// Initialize ECS game nodes
		{
			// Track mobs
			std::vector<node*> mobs = ls->get_mobs();
			ls->game_nodes.resize(mobs.size());
			for (size_t i{ 0 }; i < mobs.size(); i++)
			{
				node* n = mobs[i];
				ecs_entity_t node_entity = ecs_new(ls->world);

				ls->game_nodes[i] = {
					.entity = node_entity,
					.index = i,
					.node = n,
				};

				c_mob m{ i };
				ecs_set_id(ls->world, node_entity, ecs_id(c_mob), sizeof(c_mob), &m);

				if (n->name == "rosy")
				{
					ecs_add(ls->world, node_entity, t_rosy);
				}
			}
		}
		{
			// Track special static objects
			std::vector<node*> static_objects = ls->get_static();
			for (size_t i{ 0 }; i < static_objects.size(); i++)
			{
				node* n = static_objects[i];
				ecs_entity_t node_entity = ecs_new(ls->world);

				if (n->name == "floor")
				{
					ecs_add(ls->world, node_entity, t_floor);
					break;
				}
			}
		}
	}
	return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::update(const uint64_t delta_time)
{
	graphics_object_update_data.graphic_objects.clear();
	bool updated{ false };
	if (const auto res = ls->update(&updated, delta_time); res != result::ok)
	{
		ls->l->error("Error updating level state");
		return res;
	}
	if (!updated) return result::ok;
	graphics_object_update_data.offset = static_objects_offset;
	graphics_object_update_data.graphic_objects.resize(num_dynamic_objects);

	for (const std::vector<node*> mobs = ls->get_mobs(); const node * n : mobs) n->populate_graph(graphics_object_update_data.graphic_objects);
	return result::ok;
}