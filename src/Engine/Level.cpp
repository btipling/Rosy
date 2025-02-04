#include "Level.h"
#include "Node.h"
#include <queue>
#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/quaternion.hpp>

using namespace rosy;

constexpr size_t max_node_stack_size = 4'096;
constexpr size_t max_stack_item_list = 16'384;

const std::string mobs_node_name{ "mobs" };

namespace
{

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

	struct level_state
	{
		rosy::log* l{ nullptr };
		camera* cam{ nullptr };

		read_level_state* rls{ nullptr };
		write_level_state const* wls{ nullptr };
		node* level_game_node{ nullptr };

		rosy::result init()
		{
			level_game_node = new(std::nothrow) node;
			if (level_game_node == nullptr)
			{
				l->error("root scene_objects allocation failed");
				return result::allocation_failure;
			};
			if (const auto res = level_game_node->init(l, {}); res != result::ok)
			{
				l->error("root scene_objects initialization failed");
				return result::error;
			}
			level_game_node->name = "rosy_root";
			return result::ok;
		}

		void deinit()
		{
			if (level_game_node != nullptr) {
				level_game_node->deinit();
				delete level_game_node;
				level_game_node = nullptr;
			}
		}

		std::vector<node*> get_mobs()
		{
			if (level_game_node == nullptr) return {};
			if (level_game_node->children.empty()) return {};
			for (node* root_node = level_game_node->children[0]; node* child : root_node->children)
			{
				if (child->name == "mobs")
				{
					return child->children;
				}
			}
			return {};
		}

		rosy::result update()
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

			return result::ok;
		}
	};

	level_state* ls{ nullptr };
	scene_graph_processor* sgp{ nullptr };
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

	for (const auto& node_index : nodes) {
		const rosy_packager::node new_node = new_asset.nodes[node_index];

		auto new_game_node = new(std::nothrow) node;
		if (new_game_node == nullptr)
		{
			ls->l->error("initial scene_objects allocation failed");
			return result::allocation_failure;
		}
		if (const auto res = new_game_node->init(ls->l, new_node.transform); res != result::ok)
		{
			ls->l->error("initial scene_objects initialization failed");
			new_game_node->deinit();
			return result::error;
		}

		new_game_node->name = std::string(new_node.name.begin(), new_node.name.end());
		ls->level_game_node->children.push_back(new_game_node);
		sgp->queue.push({
			.game_node = new_game_node,
			.stack_node = new_node,
			.parent_transform = glm::mat4{1.f},
			.is_mob = false,
			});
	}

	std::vector<graphics_object> mob_graphics_objects;

	size_t go_index{ 0 };
	while (sgp->queue.size() > 0)
	{
		// ReSharper disable once CppUseStructuredBinding Visual studio wants to make this a reference, and it shouldn't be.
		stack_item queue_item = sgp->queue.front();
		sgp->queue.pop();
		assert(queue_item.game_node != nullptr);

		glm::mat4 node_transform = array_to_mat4(queue_item.stack_node.transform);
		const glm::mat4 transform = gltf_to_ndc * queue_item.parent_transform * node_transform;

		if (queue_item.stack_node.mesh_id < new_asset.meshes.size()) {
			sgp->mesh_queue.push(queue_item.stack_node.mesh_id);

			while (sgp->mesh_queue.size() > 0)
			{
				const auto current_mesh_index = sgp->mesh_queue.front();
				sgp->mesh_queue.pop();
				const rosy_packager::mesh current_mesh = new_asset.meshes[current_mesh_index];
				graphics_object go{};
				const glm::mat4 object_space_transform = glm::inverse(static_cast<glm::mat3>(transform));
				const glm::mat4 normal_transform = glm::transpose(object_space_transform);
				go.transform = mat4_to_array(transform);
				go.normal_transform = mat4_to_array(normal_transform);
				go.object_space_transform = mat4_to_array(object_space_transform);
				go.surface_data.reserve(current_mesh.surfaces.size());
				for (const auto& [sur_start_index, sur_count, sur_material] : current_mesh.surfaces)
				{
					surface_graphics_data sgd{};
					sgd.mesh_index = current_mesh_index;
					sgd.graphics_object_index = go_index;
					sgd.material_index = sur_material;
					sgd.index_count = sur_count;
					sgd.start_index = sur_start_index;
					if (new_asset.materials.size() > sur_material && new_asset.materials[sur_material].alpha_mode != 0) {
						sgd.blended = true;
					}
					go.surface_data.push_back(sgd);
				}

				for (const uint32_t child_mesh_index : current_mesh.child_meshes)
				{
					sgp->mesh_queue.push(child_mesh_index);
				}
				if (queue_item.is_mob)
				{
					mob_graphics_objects.push_back(go);
				} else
				{
					graphics_objects.push_back(go);
				}
				go_index += 1;
			}
		}

		for (const size_t child_index : queue_item.stack_node.child_nodes)
		{
			const rosy_packager::node new_node = new_asset.nodes[child_index];
			auto new_game_node = new(std::nothrow) node;
			if (new_game_node == nullptr)
			{
				ls->l->error("Error allocating new game node in set asset");
				return result::allocation_failure;
			}
			if (const auto res = new_game_node->init(ls->l, mat4_to_array(transform)); res != result::ok)
			{
				ls->l->error("Error initializing new game node in set asset");
				new_game_node->deinit();
				return result::error;
			}
			new_game_node->name = std::string(new_node.name.begin(), new_node.name.end());
			queue_item.game_node->children.push_back(new_game_node);
			const bool is_mob = queue_item.is_mob || new_game_node->name == mobs_node_name;
			sgp->queue.push({
				.game_node = new_game_node,
				.stack_node = new_node,
				.parent_transform = queue_item.parent_transform * node_transform,
				.is_mob = is_mob,
				});
		}
	}

	rls.graphic_objects.num_static_objects = graphics_objects.size();
	graphics_objects.insert(graphics_objects.end(), mob_graphics_objects.begin(), mob_graphics_objects.end());
	ls->level_game_node->debug();
	return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::update()
{
	return ls->update();
}
