// ReSharper disable CppExpressionWithoutSideEffects
#include "Level.h"
#include "Node.h"
#include "Camera.h"
#include "Editor.h"
#include "../Packager/Asset.h"
#include <queue>
#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <format>
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

const std::string mobs_node_name{"mobs"};

namespace
{
    /**** ECS DEFINITIONS ****/

    // Components
    struct c_position
    {
        [[maybe_unused]] float x{0.f};
        [[maybe_unused]] float y{0.f};
        [[maybe_unused]] float z{0.f};
    };

    struct c_mob
    {
        size_t index{0};
    };

    struct c_static
    {
        [[maybe_unused]] size_t index{0};
    };

    struct c_cursor_position
    {
        float screen_x{0.f};
        float screen_y{0.f};
    };

    struct c_target
    {
        float x{0.f};
        float y{0.f};
        float z{0.f};
        [[maybe_unused]] float t{0.f};
    };

    struct c_forward
    {
        float yaw{0.f};
    };

    struct c_pick_debugging_enabled
    {
        uint32_t space{debug_object_flag_screen_space};
    };

    // Tags
    struct [[maybe_unused]] t_rosy
    {
    };

    struct [[maybe_unused]] t_floor
    {
    };

    struct [[maybe_unused]] t_rosy_action
    {
    };

    struct [[maybe_unused]] t_pick_debugging_record
    {
    };

    struct [[maybe_unused]] t_pick_debugging_clear
    {
    };

    /**** LEVEL STATE DEFINITIONS ****/

    std::array<float, 16> mat4_to_array(glm::mat4 m)
    {
        std::array<float, 16> a{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        const auto pos_r = glm::value_ptr(m);
        for (size_t i{0}; i < 16; i++) a[i] = pos_r[i];
        return a;
    }

    std::array<float, 9> mat3_to_array(glm::mat3 m)
    {
        std::array<float, 9> a{
            1.f, 0.f, 0.f,
            0.f, 1.f, 0.f,
            0.f, 0.f, 1.f,
        };
        const auto pos_r = glm::value_ptr(m);
        for (size_t i{0}; i < 9; i++) a[i] = pos_r[i];
        return a;
    }

    glm::mat4 array_to_mat4(const std::array<float, 16>& a)
    {
        glm::mat4 m{1.f};
        const auto pos_r = glm::value_ptr(m);
        for (size_t i{0}; i < 16; i++) pos_r[i] = a[i];
        return m;
    }

    [[maybe_unused]] glm::vec4 array_to_vec4(const std::array<float, 4>& a)
    {
        glm::vec4 v{};
        const auto pos_r = glm::value_ptr(v);
        for (size_t i{0}; i < 4; i++) pos_r[i] = a[i];
        return v;
    }

    [[maybe_unused]] glm::vec3 array_to_vec3(std::array<float, 3> a)
    {
        glm::vec3 v{};
        const auto pos_r = glm::value_ptr(v);
        for (size_t i{0}; i < 3; i++) pos_r[i] = a[i];
        return v;
    }

    std::array<float, 3> vec3_to_array(glm::vec3 v)
    {
        std::array<float, 3> a{};
        for (size_t i{0}; i < 3; i++) a[i] = v[static_cast<glm::length_t>(i)];
        return a;
    }

    struct stack_item
    {
        node* game_node{nullptr};
        rosy_packager::node asset_node;
        glm::mat4 parent_transform{glm::mat4{1.f}};
        bool is_mob{false};
    };

    // Game nodes are double referenced by entity id and index in a vector.
    struct game_node_reference
    {
        flecs::entity entity;
        [[maybe_unused]] size_t index{0};
        node* node{nullptr};
    };

    constexpr float initial_fps_target{240.f};

    struct level_state
    {
        enum class camera_choice : uint8_t { game, free };

        rosy::log* l{nullptr};
        camera* game_cam{nullptr};
        camera* free_cam{nullptr};
        camera_choice active_cam{camera_choice::game};

        read_level_state* rls{nullptr};
        const write_level_state* wls{nullptr};

        editor* level_editor{nullptr};

        size_t static_objects_offset{0};
        size_t num_dynamic_objects{0};

        // asset to graphics objects
        std::queue<stack_item> queue{};

        node* level_game_node{nullptr};
        // Important game nodes that can be referenced via their graphics object index
        std::vector<game_node_reference> game_nodes;

        // ECS
        flecs::world worldz;
        flecs::entity level_entity = worldz.entity("level");
        game_node_reference rosy_reference{};
        flecs::entity floor_entity = worldz.entity("floor");

        result init(rosy::log* new_log, const config new_cfg)
        {
            l = new_log;
            if (level_game_node = new(std::nothrow) node; level_game_node == nullptr)
            {
                l->error("root scene_objects allocation failed");
                return result::allocation_failure;
            };
            if (const auto res = level_game_node->init(l, false, {}, {}, {}, {}, 1.f, 0.f); res != result::ok)
            {
                l->error("root scene_objects initialization failed");
                delete level_game_node;
                level_game_node = nullptr;
                return result::error;
            }
            level_game_node->name = "rosy_root";

            // Free camera initialization
            {
                if (free_cam = new(std::nothrow) camera{}; free_cam == nullptr)
                {
                    l->error("Error allocating camera");
                    return result::allocation_failure;
                }
                free_cam->starting_x = -5.88f;
                free_cam->starting_y = 3.86f;
                free_cam->starting_z = -1.13f;
                free_cam->starting_pitch = 0.65f;
                free_cam->starting_yaw = -1.58f;
                if (const auto res = free_cam->init(l, new_cfg); res != result::ok)
                {
                    l->error(std::format("Free camera initialization failed: {}", static_cast<uint8_t>(res)));
                    return res;
                }
            }

            // Game camera initialization
            {
                if (game_cam = new(std::nothrow) camera{}; game_cam == nullptr)
                {
                    l->error("Error allocating camera");
                    return result::allocation_failure;
                }
                game_cam->starting_x = 0.f;
                game_cam->starting_y = 10.f;
                game_cam->starting_z = -10.f;
                game_cam->starting_pitch = glm::pi<float>() / 4.f;
                game_cam->starting_yaw = 0.f;
                if (const auto res = game_cam->init(l, new_cfg); res != result::ok)
                {
                    l->error(std::format("Game camera initialization failed: {}", static_cast<uint8_t>(res)));
                    return res;
                };
            }

            // Level editor initialization
            {
                if (level_editor = new(std::nothrow) editor{}; level_editor == nullptr)
                {
                    l->error("Error allocating level editor");
                    return result::allocation_failure;
                }
                if (const auto res = level_editor->init(l, new_cfg); res != result::ok)
                {
                    l->error(std::format("Level editor initialization failed: {}", static_cast<uint8_t>(res)));
                    return res;
                };
            }

            assert(worldz.is_alive(level_entity));

            worldz.set_target_fps(initial_fps_target);

            init_systems();

            return result::ok;
        }

        void deinit()
        {
            for (const game_node_reference& gnr : game_nodes)
            {
                gnr.entity.destruct();
            }
            if (level_game_node != nullptr)
            {
                level_game_node->deinit();
                delete level_game_node;
                level_game_node = nullptr;
            }
            if (worldz.is_alive(level_entity))
            {
                level_entity.destruct();
                assert(!worldz.is_alive(level_entity));
            }
            if (level_editor)
            {
                level_editor->deinit();
                delete level_editor;
                level_editor = nullptr;
            }
            if (game_cam)
            {
                game_cam->deinit();
                delete game_cam;
                game_cam = nullptr;
            }
            if (free_cam)
            {
                free_cam->deinit();
                delete free_cam;
                free_cam = nullptr;
            }
        }

        [[nodiscard]] result reset_world()
        {
            rosy_reference = {};
            for (const game_node_reference& gnr : game_nodes)
            {
                gnr.entity.destruct();
            }
            {
                // Clear existing game nodes
                for (node* n : level_game_node->children) n->deinit();
                level_game_node->children.clear();
            }
            if (worldz.is_alive(level_entity))
            {
                level_entity.destruct();
                assert(!worldz.is_alive(level_entity));
            }
            worldz = flecs::world{};
            level_entity = worldz.entity("level");
            assert(worldz.is_alive(level_entity));

            init_systems();

            worldz.set_target_fps(initial_fps_target);

            return result::ok;
        }

        // **** ECS SYSTEM DEFINITIONS ****/

        void init_system_init_level_state() const
        {
            worldz.system("init_level_state")
                  .kind(flecs::OnLoad)
                  .run([&, this]([[maybe_unused]] flecs::iter& it)
                  {
                      {
                          // Write active camera values
                          if (std::abs(game_cam->yaw - wls->game_camera_yaw) >= 0.2)
                          {
                              game_cam->set_yaw_around_position(wls->game_camera_yaw, rosy_reference.node->get_world_space_position());
                          }
                          const camera* cam = active_cam == level_state::camera_choice::game ? game_cam : free_cam;
                          rls->cam.p = cam->p;
                          rls->cam.v = cam->v;
                          rls->cam.vp = cam->vp;
                          rls->cam.position = cam->position;
                          rls->cam.pitch = cam->pitch;
                          rls->cam.yaw = cam->yaw;
                          rls->game_camera_yaw = game_cam->yaw;
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
                          const std::vector<node*> mobs = get_mobs();
                          if (wls->mob_edit.submitted)
                          {
                              rls->mob_read.clear_edits = true;
                              if (mobs.size() > wls->mob_edit.edit_index) mobs[0]->set_world_space_translate(wls->mob_edit.position);
                          }
                          else
                          {
                              rls->mob_read.clear_edits = false;
                          }
                          rls->mob_read.mob_states.clear();
                          for (const game_node_reference& nr : game_nodes)
                          {
                              const std::array<float, 3> node_world_space_pos = nr.node->get_world_space_position();

                              std::array<float, 3> target = {0.f, 0.f, 0.f};
                              float yaw = 0.f;
                              if (nr.entity.has<c_target>())
                              {
                                  const auto tc = nr.entity.get<c_target>();
                                  target = {tc->x, tc->y, tc->z};
                              }
                              if (nr.entity.has<c_forward>())
                              {
                                  const auto fc = nr.entity.get<c_forward>();
                                  l->debug(std::format("setting rls yaw: ({:.3f}", fc->yaw));
                                  yaw = fc->yaw;
                              }
                              rls->mob_read.mob_states.push_back({
                                  .name = nr.node->name,
                                  .position = node_world_space_pos,
                                  .yaw = yaw,
                                  .target = target,
                              });
                          }
                      }
                      rls->debug_objects.clear();
                      if (level_entity.has<t_pick_debugging_clear>())
                      {
                          rls->pick_debugging.circles.clear();
                          level_entity.remove<t_pick_debugging_clear>();
                      }
                      if (level_entity.has<c_pick_debugging_enabled>())
                      {
                          const auto pick_debugging = level_entity.get<c_pick_debugging_enabled>();
                          rls->debug_objects.insert(rls->debug_objects.end(), rls->pick_debugging.circles.begin(), rls->pick_debugging.circles.end());
                          if (rls->pick_debugging.picking.has_value())
                          {
                              rls->debug_objects.push_back(rls->pick_debugging.picking.value());
                              rls->pick_debugging.picking = std::nullopt;
                          }
                          rls->pick_debugging.space = pick_debugging->space & debug_object_flag_screen_space ? pick_debug_read_state::picking_space::screen : pick_debug_read_state::picking_space::view;
                      }
                      else if (rosy_reference.node != nullptr && rosy_reference.entity.has<t_rosy_action>())
                      {
                          if (rls->pick_debugging.picking.has_value())
                          {
                              rls->debug_objects.push_back(rls->pick_debugging.picking.value());
                          }
                      }
                      else
                      {
                          rls->pick_debugging.space = pick_debug_read_state::picking_space::disabled;
                      }
                      {
                          // Light & Shadow logic
                          glm::mat4 light_sun_view;
                          glm::mat4 debug_light_sun_view;
                          glm::mat4 debug_light_translate;
                          glm::mat4 light_line_rot;
                          {
                              // Lighting math

                              {
                                  const glm::mat4 light_translate = glm::translate(glm::mat4(1.f), {0.f, 0.f, 1.f * wls->light_debug.sun_distance});
                                  debug_light_translate = glm::translate(glm::mat4(1.f), {0.f, 0.f, -1.f * wls->light_debug.sun_distance});
                                  const glm::quat pitch_rotation = angleAxis(-wls->light_debug.sun_pitch, glm::vec3{1.f, 0.f, 0.f});
                                  const glm::quat yaw_rotation = angleAxis(wls->light_debug.sun_yaw, glm::vec3{0.f, -1.f, 0.f});
                                  light_line_rot = toMat4(yaw_rotation) * toMat4(pitch_rotation);

                                  const auto camera_position = glm::vec3(light_line_rot * glm::vec4(0.f, 0.f, -wls->light_debug.sun_distance, 0.f));
                                  auto sunlight = glm::vec4(glm::normalize(camera_position), 1.f);
                                  light_sun_view = light_line_rot * light_translate;
                                  debug_light_sun_view = light_line_rot * (wls->light_debug.enable_light_perspective ? light_translate : debug_light_translate);

                                  rls->light.sunlight = {sunlight[0], sunlight[1], sunlight[2], sunlight[3]};
                              };
                          }

                          if (wls->light_debug.enable_sun_debug)
                          {
                              // Generate debug lines for light and shadow debugging
                              const glm::mat4 debug_draw_view = light_line_rot * debug_light_translate;
                              const glm::mat4 debug_light_line = glm::scale(debug_draw_view, {wls->light_debug.sun_distance, wls->light_debug.sun_distance, wls->light_debug.sun_distance});

                              debug_object line;
                              line.type = debug_object_type::line;
                              line.transform = mat4_to_array(debug_light_line);
                              line.color = {1.f, 0.f, 0.f, 1.f};
                              if (wls->light_debug.enable_sun_debug) rls->debug_objects.push_back(line);
                              {
                                  // Two circles to represent a sun
                                  constexpr float angle_step{glm::pi<float>() / 4.f};
                                  for (size_t i{0}; i < 4; i++)
                                  {
                                      debug_object sun_circle;
                                      glm::mat4 m{1.f};
                                      m = glm::rotate(m, angle_step * static_cast<float>(i), {1.f, 0.f, 0.f});
                                      sun_circle.type = debug_object_type::circle;
                                      sun_circle.transform = mat4_to_array(debug_draw_view * m);
                                      sun_circle.color = {0.976f, 0.912f, 0.609f, 1.f};
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
                              cam_lp = wls->light_debug.enable_light_perspective
                                           ? light_projections
                                           : array_to_mat4((rls->cam.p));
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
                  });
        }

        void init_system_move_rosy()
        {
            worldz.system<t_rosy_action>("move_rosy")
                  .kind(flecs::OnUpdate)
                  .each([&, this](flecs::iter& it, size_t, t_rosy_action)
                  {
                      // This function moves rosy to where the screen cursor is when the left mouse button is pushed.
                      if (!level_entity.has<c_cursor_position>()) return;

                      // Start picking. Picking here works by converting the 2D screen space cursor position to Vulkan NDC coordinates and then converts them
                      // to clip space. From clip space their position is on the projection plane in view space. A ray is created from the click position on the projection plane from
                      // the view space camera position, which is just origin. The ray is transformed to world space, the floor's origin is the same as the world space origin.
                      // Plucker coordinates are used to calculate the ray's intersection in the floor plane, as derived from Foundations of Game Engine Development [Lengyel]
                      const auto c_pos = level_entity.get<c_cursor_position>();
                      const camera* cam = active_cam == level_state::camera_choice::game ? game_cam : free_cam;

                      auto camera_pos = glm::vec3(glm::vec4(cam->position[0], cam->position[1], cam->position[2], 1.f));

                      // Get the values used to transform the screen coordinates to view space.
                      const float a = static_cast<float>(cam->s);
                      const float w = cam->viewport_width;
                      const float h = cam->viewport_height;
                      const float fov = static_cast<float>(cam->fov) / 100.f;
                      const float x_s = c_pos->screen_x / w;
                      const float y_s = c_pos->screen_y / h;
                      const float g = static_cast<float>(cam->g);

                      // This uses NDC + accounts for field of view and perspective to put x and y into view space.
                      const float x_v = (((2.f * x_s) - 1.f) * a) * fov;
                      const float y_v = (2.f * y_s - 1.f) * fov;

                      // This is the click at actually twice the value of the projection plane distance.
                      const auto view_click = glm::vec3(x_v, -y_v, 2.f * g);
                      // This transforms the view click into world space using the inverse the view matrix.
                      const auto world_ray = glm::vec3(glm::inverse(array_to_mat4(cam->v)) * glm::vec4(view_click, 0.f));

                      // Create the pucker coordinates v and m;
                      const glm::vec3 plucker_v = world_ray;
                      const glm::vec3 plucker_m = cross(camera_pos, world_ray);

                      // Create the parameterized plane values for the floor in world space.
                      constexpr auto normal = glm::vec3(0.f, 1.f, 0.f);
                      constexpr float plane_distance = 0.f;

                      // Calculate the intersection using the homogenous plane intersection formula
                      const glm::vec3 m_x_n = glm::cross(plucker_m, normal);
                      const glm::vec3 d_v = plucker_v * plane_distance;
                      const glm::vec3 intersection = m_x_n + d_v;
                      const float intersection_w = glm::dot(-normal, plucker_v);

                      l->debug(std::format("intersection {:.3f}, {:.3f}, {:.3f}", intersection[0] / intersection_w, intersection[1] / intersection_w, intersection[2] / intersection_w));

                      // Set rosy to target that intersection.
                      auto rosy_target = glm::vec3(intersection[0] / intersection_w, intersection[1] / intersection_w, intersection[2] / intersection_w);

                      // This is all taking place in world space. It used to be in object space and that was incorrect because this is about moving rosy in world space.
                      // Calculate whether the target is within the floor's bounds, if not set any coordinate outside to the max extent of the bounds.
                      const auto floor_index = floor_entity.get<c_static>();
                      const node* floor_node = get_static()[floor_index->index];
                      auto world_space_floor_bounds = floor_node->get_world_space_bounds();
                      for (int j{0}; j < 3; j++)
                      {
                          if (rosy_target[j] < world_space_floor_bounds.min[j])
                          {
                              rosy_target[j] = world_space_floor_bounds.min[j];
                          }
                          if (rosy_target[j] > world_space_floor_bounds.max[j])
                          {
                              rosy_target[j] = world_space_floor_bounds.max[j];
                          }
                      }

                      c_target target{.x = rosy_target.x, .y = rosy_target.y, .z = rosy_target.z};
                      rosy_reference.entity.add<c_target>().set<c_target>(target);

                      // Draw some debugging UI to display picking performance.
                      if (level_entity.has<c_pick_debugging_enabled>())
                      {
                          const auto pick_debugging = level_entity.get<c_pick_debugging_enabled>();
                          glm::mat4 m;
                          std::array<float, 4> color;
                          if (pick_debugging->space & debug_object_flag_screen_space)
                          {
                              // Draw a green circle to indicate screen space coordinate system is being used.
                              color = {0.f, 1.f, 0.f, 1.f};
                              m = glm::translate(glm::mat4(1.f), glm::vec3(x_s * 2.f - 1.f, y_s * 2.f - 1.f, 0.1f));
                          }
                          else
                          {
                              // Else we're in view space and want to make sure our circle is in the correct location for view space to show we're tracking the mouse cursor in view space correctly.
                              color = {1.f, 1.f, 0.f, 1.f};
                              m = glm::translate(glm::mat4(1.f), view_click);
                          }
                          m = glm::scale(m, glm::vec3(0.01f));
                          // This will draw a little green ir yellow circle depending on which space we are drawing the circle on the screen.
                          rls->pick_debugging.picking = {
                              .type = debug_object_type::circle,
                              .transform = mat4_to_array(m),
                              .color = color,
                              .flags = pick_debugging->space,
                          };
                          // Record the ray as a debug line to display:
                          if (level_entity.has<t_pick_debugging_record>())
                          {
                              float distance{1000.f}; // It's a long ray.
                              glm::vec3 draw_location = world_ray * distance;
                              distance += 2.f;
                              // We use the transform matrix as the actual points to render the line to via a flag.
                              auto m2 = glm::mat4(
                                  glm::vec4(camera_pos, 1.f),
                                  glm::vec4(draw_location, 1.f),
                                  glm::vec4(1.f),
                                  glm::vec4(1.f)
                              );
                              rls->pick_debugging.circles.push_back({
                                  .type = debug_object_type::line,
                                  .transform = mat4_to_array(m2),
                                  .color = {0.f, 1.f, 0.f, 1.f},
                                  .flags = debug_object_flag_transform_is_points,
                              });
                              level_entity.remove<t_pick_debugging_record>();
                          }
                      }
                      else
                      {
                          // In this case, if we're in edit mode we will draw nice little cursor on the actual floor where rosy is targeting.
                          glm::mat4 circle_m = glm::translate(glm::mat4(1.f), glm::vec3(intersection[0] / intersection_w, (intersection[1] / intersection_w) + 0.1f, intersection[2] / intersection_w));
                          circle_m = glm::rotate(circle_m, (glm::pi<float>() / 2.f), glm::vec3(1.f, 0.f, 0.f));
                          circle_m = glm::scale(circle_m, glm::vec3(0.25f));

                          rls->pick_debugging.picking = {
                              .type = debug_object_type::circle,
                              .transform = mat4_to_array(circle_m),
                              .color = {0.f, 1.f, 0.f, 1.f},
                              .flags = 0,
                          };
                      }
                      // If rosy is targeting orient rosy and move her toward the target.
                      if (rosy_reference.entity.has<c_target>())
                      {
                          const std::array<float, 3> node_world_space_position = rosy_reference.node->get_world_space_position();
                          const auto rosy_pos = glm::vec3(node_world_space_position[0], node_world_space_position[1], node_world_space_position[2]);
                          constexpr auto game_forward = glm::vec3(0.f, 0.f, 1.f);

                          // Convert rosy target translation to world position if rosy's position was world origin.
                          // This works because rosy will be at origin in this space and so wherever the target is at an angle for rosy's position.
                          // This is not rosy's asset-origin space, this just translating world origin to get a world yaw for rosy.
                          glm::mat4 transform_world_origin_to_rosy_position = glm::translate(glm::mat4(1.f), {-rosy_pos[0], -rosy_pos[1], -rosy_pos[2]});
                          // Verify that the rosy_space target isn't the null vector, if it is she's already there.
                          if (glm::vec4 rosy_target_if_rosy_is_world_origin = transform_world_origin_to_rosy_position * glm::vec4(rosy_target, 1.f); rosy_target_if_rosy_is_world_origin != glm::zero<
                              glm::vec4>())
                          {
                              // Calculate the difference between rosy's orientation and her target position
                              const float sign = rosy_target_if_rosy_is_world_origin.x > 0 ? -1.f : 1.f;
                              // This offset's rosy's orientation based on her orientation around the x-axis.
                              const float target_game_cos_theta = glm::dot(glm::normalize(glm::vec3(rosy_target_if_rosy_is_world_origin)), game_forward);
                              // This dot product is used to get the angle of the target with respect to rosy's position.
                              const float target_yaw = sign * std::acos(target_game_cos_theta);

                              l->debug(std::format(
                                  "rosy_target: ({:.3f}, {:.3f}, {:.3f}) cosTheta {:.3f}) target_yaw: {:.3f}) sign: {:.3f}",
                                  rosy_target_if_rosy_is_world_origin[0],
                                  rosy_target_if_rosy_is_world_origin[1],
                                  rosy_target_if_rosy_is_world_origin[2],
                                  target_game_cos_theta,
                                  target_yaw,
                                  sign
                              ));

                              c_forward nf{.yaw = target_yaw};
                              rosy_reference.entity.add<c_forward>().set<c_forward>(nf);

                              // Linearly interpolate rosy's position toward the target
                              const float t = 1.f * it.delta_time();
                              glm::vec3 new_rosy_pos = (rosy_pos * (1.f - t)) + rosy_target * t;

                              // Update rosy's world space orientation and position
                              rosy_reference.node->set_world_space_translate(vec3_to_array(new_rosy_pos));
                              // Set rosy's orientation to face target
                              rosy_reference.node->set_world_space_yaw(target_yaw);

                              game_cam->set_game_cam_position({new_rosy_pos[0], new_rosy_pos[1], new_rosy_pos[2]});
                          }
                      }
                  });
        }

        void init_systems()
        {
            init_system_init_level_state();
            init_system_move_rosy();
        }

        [[nodiscard]] std::vector<node*> get_mobs() const
        {
            if (level_game_node == nullptr) return {};
            if (level_game_node->children.empty()) return {};
            for (const node* root_node = level_game_node->children[0]; node* child : root_node->children)
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
            for (const node* root_node = level_game_node->children[0]; node* child : root_node->children)
            {
                if (child->name == "static")
                {
                    return child->children;
                }
            }
            return {};
        }

        [[nodiscard]] result setup_frame() const
        {
            rls->go_update.graphic_objects.clear();
            if (rls->target_fps != wls->target_fps)
            {
                worldz.set_target_fps(wls->target_fps);
                rls->target_fps = wls->target_fps;
            }
            return result::ok;
        }

        [[nodiscard]] result update(const uint32_t viewport_width, const uint32_t viewport_height,
                                    const double dt)
        {
            if (const auto res = level_editor->process(wls->editor_commands, &rls->editor_state); res != result::ok)
            {
                l->error(std::format("Error processing editor commands {}", static_cast<uint8_t>(res)));
                return res;
            }
            if (rls->editor_state.new_asset != nullptr)
            {
                const auto a = static_cast<const rosy_packager::asset*>(rls->editor_state.new_asset);
                const result res = set_asset(*a);
                if (res != result::ok)
                {
                    l->error(std::format("Error setting new asset {}", static_cast<uint8_t>(res)));
                }
                return res;
            }
            camera* cam = active_cam == level_state::camera_choice::game ? game_cam : free_cam;
            if (const auto res = cam->update(viewport_width, viewport_height, dt); res != result::ok)
            {
                l->error("Error updating camera");
                return res;
            }
            worldz.progress(static_cast<float>(dt));
            return result::ok;
        }

        [[nodiscard]] result process_sdl_event(const SDL_Event& event)
        {
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_F1)
                {
                    rls->ui_enabled = !rls->ui_enabled;
                }
                if (event.key.key == SDLK_F2)
                {
                    active_cam = active_cam == camera_choice::game ? camera_choice::free : camera_choice::game;
                    if (active_cam == camera_choice::game) rls->cursor_enabled = true;
                }
            }

            if (active_cam == camera_choice::free)
            {
                if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    if (event.key.key == SDLK_C) rls->cursor_enabled = !rls->cursor_enabled;
                    if (event.key.key == SDLK_W) free_cam->move(camera::direction::z_pos, 1.f);
                    if (event.key.key == SDLK_S) free_cam->move(camera::direction::z_neg, 1.f);
                    if (event.key.key == SDLK_A) free_cam->move(camera::direction::x_neg, 1.f);
                    if (event.key.key == SDLK_D) free_cam->move(camera::direction::x_pos, 1.f);
                    if (event.key.key == SDLK_SPACE) free_cam->move(camera::direction::y_pos, 1.f);
                    if (event.key.key == SDLK_Z) free_cam->move(camera::direction::y_neg, 1.f);
                    if (event.key.mod & SDL_KMOD_SHIFT) free_cam->go_fast();
                }
                if (event.type == SDL_EVENT_KEY_UP)
                {
                    if (event.key.key == SDLK_W) free_cam->move(camera::direction::z_pos, 0.f);
                    if (event.key.key == SDLK_S) free_cam->move(camera::direction::z_neg, 0.f);
                    if (event.key.key == SDLK_A) free_cam->move(camera::direction::x_neg, 0.f);
                    if (event.key.key == SDLK_D) free_cam->move(camera::direction::x_pos, 0.f);
                    if (event.key.key == SDLK_SPACE) free_cam->move(camera::direction::y_pos, 0.f);
                    if (event.key.key == SDLK_Z) free_cam->move(camera::direction::y_neg, 0.f);
                    if (!(event.key.mod & SDL_KMOD_SHIFT)) free_cam->go_slow();
                }
                if (!rls->cursor_enabled && event.type == SDL_EVENT_MOUSE_MOTION)
                {
                    free_cam->yaw_in_dir(event.motion.xrel / 250.f);
                    free_cam->pitch_in_dir(event.motion.yrel / 250.f);
                }
            }
            else if (rosy_reference.node != nullptr && active_cam == camera_choice::game)
            {
                if (event.type == SDL_EVENT_MOUSE_WHEEL)
                {
                    rls->game_camera_yaw = wls->game_camera_yaw;
                    const auto mbe = reinterpret_cast<const SDL_MouseWheelEvent&>(event);
                    const float direction = mbe.direction == SDL_MOUSEWHEEL_NORMAL ? 1.f : -1.f;
                    float new_yaw = rls->game_camera_yaw + mbe.y / 25.f * direction;
                    if (new_yaw < 0.f) new_yaw += glm::pi<float>() * 2;
                    if (new_yaw >= glm::pi<float>() * 2) new_yaw -= glm::pi<float>() * 2;
                    game_cam->set_yaw_around_position(new_yaw, rosy_reference.node->get_world_space_position());
                }
            }

            constexpr Uint8 rosy_attention_btn{1};
            constexpr Uint8 pick_debug_toggle_btn{2};
            constexpr Uint8 pick_debug_record_btn{3};
            const flecs::entity rosy_entity = rosy_reference.entity;
            if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type ==
                SDL_EVENT_MOUSE_BUTTON_UP)
            {
                const auto mbe = reinterpret_cast<const SDL_MouseButtonEvent&>(event);
                if (std::isnan(mbe.x) || (std::isnan(mbe.y))) return result::ok;
                const c_cursor_position c_pos{.screen_x = mbe.x, .screen_y = mbe.y};
                level_entity.add<c_cursor_position>().set(c_pos);
            }
            if (rosy_reference.node != nullptr && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                switch (const auto mbe = reinterpret_cast<const SDL_MouseButtonEvent&>(event); mbe.button)
                {
                case rosy_attention_btn:
                    rosy_entity.add<t_rosy_action>();
                    break;
                case pick_debug_toggle_btn:
                    if (mbe.clicks == 1 && level_entity.has<c_pick_debugging_enabled>())
                    {
                        level_entity.remove<c_pick_debugging_enabled>();
                        level_entity.add<t_pick_debugging_clear>();
                    }
                    else
                    {
                        const uint32_t space = mbe.clicks > 2 ? debug_object_flag_view_space : debug_object_flag_screen_space;
                        const c_pick_debugging_enabled pick{.space = space};
                        level_entity.add<c_pick_debugging_enabled>().set(pick);
                    }
                    break;
                case pick_debug_record_btn:
                    level_entity.add<t_pick_debugging_record>();
                    break;
                default:
                    break;
                }
            }
            if (rosy_reference.node != nullptr && event.type == SDL_EVENT_MOUSE_BUTTON_UP)
            {
                if (const auto mbe = reinterpret_cast<const SDL_MouseButtonEvent&>(event); mbe.button == rosy_attention_btn)
                {
                    rosy_entity.remove<t_rosy_action>();
                }
            }
            return result::ok;
        }


        result set_asset(const rosy_packager::asset& new_asset)
        {
            if (const result res = reset_world(); res != result::ok)
            {
                l->error("error resetting world in set_asset");
                return res;
            }
            // Traverse the assets to construct the scene graph and track important game play entities.
            rls->go_update.full_scene.clear();
            const size_t root_scene_index = static_cast<size_t>(new_asset.root_scene);
            if (new_asset.scenes.size() <= root_scene_index)
            {
                l->error(std::format("error new_asset.scenes.size() <= root_scene_index: {} <= {}", new_asset.scenes.size(), root_scene_index));
                return result::invalid_argument;
            }

            const auto& scene = new_asset.scenes[root_scene_index];
            if (scene.nodes.empty())
            {
                l->error("error scene.nodes.empty()");
                return result::invalid_argument;
            }

            const std::array<float, 16> asset_coordinate_system_transform = new_asset.asset_coordinate_system;

            {
                // Reset state
                while (!queue.empty()) queue.pop();
            }

            // Prepopulate the node queue with the root scenes nodes
            for (const auto& node_index : scene.nodes)
            {
                const rosy_packager::node new_node = new_asset.nodes[node_index];

                // Game nodes are a game play representation of a graphics object, and can be static or a mob.
                auto new_game_node = new(std::nothrow) node;
                if (new_game_node == nullptr)
                {
                    l->error("initial scene_objects allocation failed");
                    return result::allocation_failure;
                }

                // The initial parent transform to populate the scene graph hierarchy is an identity matrix.
                const std::array<float, 16> identity_m = mat4_to_array(glm::mat4(1.f));

                // All nodes must be initialized here and below.
                if (const auto res = new_game_node->init(l, false, asset_coordinate_system_transform, identity_m, new_node.transform, new_node.world_translate,
                                                         new_node.world_scale, new_node.world_yaw); res !=
                    result::ok)
                {
                    l->error("initial scene_objects initialization failed");
                    new_game_node->deinit();
                    delete new_game_node;
                    delete new_game_node;
                    return result::error;
                }

                // All nodes have a name.
                new_game_node->name = std::string(new_node.name.begin(), new_node.name.end());

                // Root nodes are directly owned by the level state.
                level_game_node->children.push_back(new_game_node);

                // Populate the node queue.
                queue.push({
                    .game_node = new_game_node,
                    .asset_node = new_node,
                    .parent_transform = glm::mat4{1.f},
                    .is_mob = false, // Root nodes are assumed to be static.
                });
            }

            std::vector<graphics_object> mob_graphics_objects;

            // Use two indices to track either static graphics objects or dynamic "mobs"
            size_t go_mob_index{0};
            size_t go_static_index{0};
            while (queue.size() > 0)
            {
                // ReSharper disable once CppUseStructuredBinding Visual studio wants to make this a reference, and it shouldn't be.
                stack_item queue_item = queue.front();
                queue.pop();
                assert(queue_item.game_node != nullptr);


                const glm::mat4 node_object_space_transform = array_to_mat4(queue_item.game_node->get_object_space_transform());
                const glm::mat4 node_world_space_transform = array_to_mat4(queue_item.game_node->get_world_space_transform());

                // Set node bounds for bound testing.
                node_bounds object_space_bounds{};
                // Advance the next item in the node queue
                if (queue_item.asset_node.mesh_id < new_asset.meshes.size())
                {
                    // Each node has a mesh id. 
                    const auto current_mesh_index = queue_item.asset_node.mesh_id;

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
                        const glm::mat4 to_object_space_transform = glm::inverse(static_cast<glm::mat4>(node_world_space_transform));
                        const glm::mat3 normal_transform = glm::transpose(glm::inverse(glm::mat3(node_world_space_transform)));

                        go.transform = mat4_to_array(node_world_space_transform);
                        go.to_object_space_transform = mat4_to_array(to_object_space_transform);
                        go.normal_transform = mat3_to_array(normal_transform);
                    }

                    {
                        // Each mesh has any number of surfaces that are derived from gltf primitives for example. These are what are given to the renderer to draw.
                        go.surface_data.reserve(current_mesh.surfaces.size());
                        for (const auto& surf : current_mesh.surfaces)
                        {
                            glm::vec4 object_space_min_bounds = {surf.min_bounds[0], surf.min_bounds[1], surf.min_bounds[2], 1.f};
                            glm::vec4 object_space_max_bounds = {surf.max_bounds[0], surf.max_bounds[1], surf.max_bounds[2], 1.f};
                            for (glm::length_t i{0}; i < 3; i++)
                            {
                                object_space_bounds.min[i] = glm::min(object_space_min_bounds[i], object_space_bounds.min[i]);
                                object_space_bounds.max[i] = glm::max(object_space_max_bounds[i], object_space_bounds.max[i]);
                            }
                            queue_item.game_node->set_object_space_bounds(object_space_bounds);
                            surface_graphics_data sgd{};
                            sgd.mesh_index = current_mesh_index;
                            // The index written to the surface graphic object is critical for pulling its transforms out of the buffer for rendering.
                            // The two separate indices indicate where renderer's buffer on the GPU its data will live. An offset is used for
                            // mobs that is equal to the total number of static surface graphic objects as mobs are all at the end of the buffer.
                            // The go_mob_index is also used to identify individual mobs for game play purposes.
                            sgd.graphics_object_index = queue_item.is_mob ? go_mob_index : go_static_index;
                            sgd.material_index = surf.material;
                            sgd.index_count = surf.count;
                            sgd.start_index = surf.start_index;
                            if (new_asset.materials.size() > surf.material && new_asset.materials[surf.material].
                                alpha_mode
                                != 0)
                            {
                                sgd.blended = true;
                            }
                            go.surface_data.push_back(sgd);
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
                            rls->go_update.full_scene.push_back(go);
                            go_static_index += 1;
                        }
                        // Also track all the graphic objects in a combined bucket.
                        queue_item.game_node->graphics_objects.push_back(go);
                    }
                }

                // Each node can have an arbitrary number of child nodes.
                for (const size_t child_index : queue_item.asset_node.child_nodes)
                {
                    // This is the same node initialization sequence from the root's scenes logic above.
                    const rosy_packager::node new_asset_node = new_asset.nodes[child_index];

                    // Create the pointer
                    auto new_game_node = new(std::nothrow) node;
                    if (new_game_node == nullptr)
                    {
                        l->error("Error allocating new game node in set asset");
                        return result::allocation_failure;
                    }

                    std::array<float, 16> node_coordinate_system = asset_coordinate_system_transform;
                    // Check if the node has its own coordinate system (because the nodes in the assets may come from different coordinate systems).
                    for (const float v : new_asset_node.coordinate_system)
                    {
                        if (std::abs(v) > 0.000001)
                        {
                            node_coordinate_system = new_asset_node.coordinate_system;
                            break;
                        }
                    }

                    // Initialize its state
                    if (const auto res = new_game_node->init(l,
                                                             new_asset_node.is_world_node,
                                                             node_coordinate_system,
                                                             mat4_to_array(node_object_space_transform),
                                                             new_asset_node.transform,
                                                             new_asset_node.world_translate,
                                                             new_asset_node.world_scale,
                                                             new_asset_node.world_yaw);
                        res != result::ok)
                    {
                        l->error("Error initializing new game node in set asset");
                        new_game_node->deinit();
                        delete new_game_node;
                        return result::error;
                    }

                    // Give it a name
                    new_game_node->name = std::string(new_asset_node.name.begin(), new_asset_node.name.end());

                    // New nodes are recorded as children of their parent to form a scene graph.
                    queue_item.game_node->children.push_back(new_game_node);

                    // Mobs are a child of "mob" node or are ancestors of the "mob" node's children
                    const bool is_mob = queue_item.is_mob || new_game_node->name == mobs_node_name;

                    // Add the node to the node queue to have their meshes and primitives processed.
                    queue.push({
                        .game_node = new_game_node,
                        .asset_node = new_asset_node,
                        .parent_transform = queue_item.parent_transform * node_object_space_transform,
                        .is_mob = is_mob,
                    });
                }
            }

            {
                // Record the indices of static and mob graphics objects for rendering and other uses as explained above.
                rls->graphic_objects.static_objects_offset = go_static_index;
                static_objects_offset = go_static_index;
                num_dynamic_objects = go_mob_index;
            }
            {
                // It is at this point that the total number of static graphic object is known. Traverse all the mobs graphic object to record that offset.
                for (size_t i{0}; i < mob_graphics_objects.size(); i++)
                {
                    for (size_t j{0}; j < mob_graphics_objects[i].surface_data.size(); j++)
                    {
                        mob_graphics_objects[i].surface_data[j].graphic_objects_offset = static_objects_offset;
                    }
                }
            }
            {
                // Merge the static objects and mob graphic objects into one vector to be sent to the renderer for the initial upload to the gpu
                rls->go_update.full_scene.insert(rls->go_update.full_scene.end(), mob_graphics_objects.begin(),
                                                 mob_graphics_objects.end());
            }
            {
                // Print the result of all this if debug logging is on.
                level_game_node->debug();
            }
            {
                // Initialize ECS game nodes
                {
                    // Track mobs
                    std::vector<node*> mobs = get_mobs();
                    game_nodes.resize(mobs.size());
                    for (size_t i{0}; i < mobs.size(); i++)
                    {
                        node* n = mobs[i];
                        flecs::entity node_entity = worldz.entity();

                        game_node_reference ref = {
                            .entity = node_entity,
                            .index = i,
                            .node = n,
                        };
                        game_nodes[i] = ref;

                        c_mob m{i};
                        node_entity.add<c_mob>().set(m);
                        c_forward forward{.yaw = 0.f};
                        node_entity.add<c_forward>().set(forward);

                        if (n->name == "rosy")
                        {
                            rosy_reference = ref;
                            node_entity.add<t_rosy>();
                            game_cam->set_game_cam_position(rosy_reference.node->get_world_space_position());
                        }
                    }
                }
                {
                    // Track special static objects
                    std::vector<node*> static_objects = get_static();
                    for (size_t i{0}; i < static_objects.size(); i++)
                    {
                        node* n = static_objects[i];
                        flecs::entity node_entity = worldz.entity();

                        if (n->name == "floor")
                        {
                            floor_entity = node_entity;
                            c_static m{i};
                            node_entity.add<t_floor>().set(m);
                            break;
                        }
                    }
                }
            }
            return result::ok;
        }
    };

    level_state* ls{nullptr};
}

result level::init(log* new_log, const config new_cfg)
{
    {
        // Init level state
        {
            wls.light_debug.sun_distance = 23.776f;
            wls.light_debug.sun_pitch = 5.538f;
            wls.light_debug.sun_yaw = 6.414f;
            wls.light_debug.orthographic_depth = 76.838f;
            wls.light_debug.cascade_level = 36.f;
            wls.light.depth_bias_constant = -21.882f;
            wls.light.depth_bias_clamp = -20.937f;
            wls.light.depth_bias_slope_factor = -3.922f;
            wls.draw_config.cull_enabled = true;
            wls.light.sunlight_color = {0.55f, 0.55f, 0.55f, 1.f};
            wls.light.ambient_light = 0.04f;
            wls.light.depth_bias_enabled = true;
            wls.draw_config.thick_wire_lines = false;
            wls.fragment_config.output = 0;
            wls.fragment_config.light_enabled = true;
            wls.fragment_config.tangent_space_enabled = true;
            wls.fragment_config.shadows_enabled = true;
            wls.fragment_config.normal_maps_enabled = true;
            wls.target_fps = initial_fps_target;
            rls.ui_enabled = true;
        }
        if (ls = new(std::nothrow) level_state; ls == nullptr)
        {
            new_log->error("level_state allocation failed");
            return result::allocation_failure;
        }
        if (const auto res = ls->init(new_log, new_cfg); res != result::ok)
        {
            new_log->error("level_state init failed");
            return res;
        }

        ls->rls = &rls;
        ls->wls = &wls;
    }
    return result::ok;
}


// ReSharper disable once CppMemberFunctionMayBeStatic
void level::deinit()
{
    if (ls)
    {
        ls->deinit();
        delete ls;
        ls = nullptr;
    }
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::setup_frame()
{
    return ls->setup_frame();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::update(const uint32_t viewport_width, const uint32_t viewport_height, const double dt)
{
    if (const auto res = ls->update(viewport_width, viewport_height, dt); res != result::ok)
    {
        ls->l->error("Error updating level state");
        return res;
    }
    return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::process()
{
    ls->rls->go_update.offset = ls->static_objects_offset;
    ls->rls->go_update.graphic_objects.resize(ls->num_dynamic_objects);

    for (const std::vector<node*> mobs = ls->get_mobs(); const node* n : mobs)
    {
        n->populate_graph(ls->rls->go_update.graphic_objects);
    }
    return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::process_sdl_event(const SDL_Event& event)
{
    return ls->process_sdl_event(event);
}
