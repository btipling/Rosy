#include "Level.h"
#include "Node.h"
#include "Camera.h"
#include "Editor.h"
#include <print>
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

    ECS_COMPONENT_DECLARE(c_position);

    struct c_mob
    {
        size_t index{0};
    };

    ECS_COMPONENT_DECLARE(c_mob);

    struct c_static
    {
        [[maybe_unused]] size_t index{0};
    };

    ECS_COMPONENT_DECLARE(c_static);

    struct c_cursor_position
    {
        float screen_x{0.f};
        float screen_y{0.f};
    };

    ECS_COMPONENT_DECLARE(c_cursor_position);

    struct c_target
    {
        float x{0.f};
        float y{0.f};
        float z{0.f};
        [[maybe_unused]] float t{0.f};
    };

    ECS_COMPONENT_DECLARE(c_target);

    struct c_forward
    {
        float yaw{0.f};
    };

    ECS_COMPONENT_DECLARE(c_forward);

    struct c_pick_debugging_enabled
    {
        uint32_t space{debug_object_flag_screen_space};
    };

    ECS_COMPONENT_DECLARE(c_pick_debugging_enabled);

    // Tags
    struct [[maybe_unused]] t_rosy
    {
    };

    ECS_TAG_DECLARE(t_rosy);

    struct [[maybe_unused]] t_floor
    {
    };

    ECS_TAG_DECLARE(t_floor);

    struct [[maybe_unused]] t_rosy_action
    {
    };

    ECS_TAG_DECLARE(t_rosy_action);

    struct [[maybe_unused]] t_pick_debugging_record
    {
    };

    ECS_TAG_DECLARE(t_pick_debugging_record);

    struct [[maybe_unused]] t_pick_debugging_clear
    {
    };

    ECS_TAG_DECLARE(t_pick_debugging_clear);

    // System definitions are forward declared and defined at the end.
    void detect_mob(ecs_iter_t* it);
    void detect_floor(ecs_iter_t* it);
    void init_level_state(ecs_iter_t* it);
    void move_rosy(ecs_iter_t* it);

    /**** LEVEL STATE DEFINITIONS ****/

    std::array<float, 16> mat4_to_array(glm::mat4 m)
    {
        std::array<float, 16> a{};
        const auto pos_r = glm::value_ptr(m);
        for (uint64_t i{0}; i < 16; i++) a[i] = pos_r[i];
        return a;
    }

    glm::mat4 array_to_mat4(const std::array<float, 16>& a)
    {
        glm::mat4 m{};
        const auto pos_r = glm::value_ptr(m);
        for (uint64_t i{0}; i < 16; i++) pos_r[i] = a[i];
        return m;
    }

    [[maybe_unused]] glm::vec4 array_to_vec4(const std::array<float, 4>& a)
    {
        glm::vec4 v{};
        const auto pos_r = glm::value_ptr(v);
        for (uint64_t i{0}; i < 4; i++) pos_r[i] = a[i];
        return v;
    }

    struct stack_item
    {
        node* game_node{nullptr};
        rosy_packager::node stack_node;
        glm::mat4 parent_transform{glm::mat4{1.f}};
        bool is_mob{false};
    };

    struct scene_graph_processor
    {
        std::queue<stack_item> queue{};
        std::queue<uint32_t> mesh_queue{};
    };

    // Game nodes are double referenced by entity id and index in a vector.
    struct game_node_reference
    {
        ecs_entity_t entity{0};
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

        node* level_game_node{nullptr};
        // Important game nodes that can be referenced via their graphics object index
        std::vector<game_node_reference> game_nodes;

        // ECS
        ecs_world_t* world{nullptr};
        ecs_entity_t level_entity{0};
        game_node_reference rosy_reference{};
        ecs_entity_t floor_entity{0};

        result init(rosy::log* new_log, const config new_cfg)
        {
            l = new_log;
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

            // Free camera initialization
            {
                free_cam = new(std::nothrow) camera{};
                if (free_cam == nullptr)
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
                game_cam = new(std::nothrow) camera{};
                if (game_cam == nullptr)
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
                level_editor = new(std::nothrow) editor{};
                if (level_editor == nullptr)
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

            world = ecs_init();
            if (world == nullptr)
            {
                l->error("flecs world init failed");
                return result::error;
            }
            level_entity = ecs_new(world);
            assert(ecs_is_alive(world, level_entity));

            ecs_set_target_fps(world, initial_fps_target);

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
            if (ecs_is_alive(world, level_entity))
            {
                ecs_delete(world, level_entity);
                assert(!ecs_is_alive(world, level_entity));
            }
            if (world != nullptr)
            {
                ecs_fini(world);
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
            if (level_game_node != nullptr)
            {
                level_game_node->deinit();
                delete level_game_node;
                level_game_node = nullptr;
            }
        }

        void init_components() const
        {
            ECS_COMPONENT_DEFINE(world, c_position);
            ECS_COMPONENT_DEFINE(world, c_mob);
            ECS_COMPONENT_DEFINE(world, c_static);
            ECS_COMPONENT_DEFINE(world, c_cursor_position);
            ECS_COMPONENT_DEFINE(world, c_target);
            ECS_COMPONENT_DEFINE(world, c_forward);
            ECS_COMPONENT_DEFINE(world, c_pick_debugging_enabled);
        }

        void init_tags() const
        {
            ECS_TAG_DEFINE(world, t_rosy);
            ECS_TAG_DEFINE(world, t_floor);
            ECS_TAG_DEFINE(world, t_rosy_action);
            ECS_TAG_DEFINE(world, t_pick_debugging_record);
            ECS_TAG_DEFINE(world, t_pick_debugging_clear);
        }

        void init_systems()
        {
            {
                // Load initial writable state from renderer
                ecs_system_desc_t desc{};
                {
                    ecs_entity_desc_t e_desc{};
                    e_desc.id = 0;
                    e_desc.name = "init_level_state";
                    {
                        ecs_id_t add_ids[3]{};
                        add_ids[0] = {ecs_dependson(flecs::OnLoad)};
                        add_ids[1] = flecs::OnLoad;
                        add_ids[2] = 0;
                        e_desc.add = add_ids;
                    }
                    desc.entity = ecs_entity_init(world, &e_desc);
                }
                desc.ctx = static_cast<void*>(this);
                desc.callback = init_level_state;
                ecs_system_init(world, &desc);
            }
            {
                // Detect mob
                ecs_system_desc_t desc{};
                {
                    ecs_entity_desc_t e_desc{};
                    e_desc.id = 0;
                    e_desc.name = "detect_mob";
                    {
                        ecs_id_t add_ids[3]{};
                        add_ids[0] = {ecs_dependson(EcsOnUpdate)};
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
                // Detect floor
                ecs_system_desc_t desc{};
                {
                    ecs_entity_desc_t e_desc{};
                    e_desc.id = 0;
                    e_desc.name = "detect_floor";
                    {
                        ecs_id_t add_ids[3]{};
                        add_ids[0] = {ecs_dependson(EcsOnUpdate)};
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
            {
                // Move rosy
                ecs_system_desc_t desc{};
                {
                    ecs_entity_desc_t e_desc{};
                    e_desc.id = 0;
                    e_desc.name = "move_rosy";
                    {
                        ecs_id_t add_ids[3]{};
                        add_ids[0] = {ecs_dependson(EcsOnUpdate)};
                        add_ids[1] = EcsOnUpdate;
                        add_ids[2] = 0;
                        e_desc.add = add_ids;
                    }
                    desc.entity = ecs_entity_init(world, &e_desc);
                }
                desc.query.expr = "t_rosy_action";
                desc.ctx = static_cast<void*>(this);
                desc.callback = move_rosy;
                ecs_system_init(world, &desc);
            }
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
            if (rls->target_fps != wls->target_fps)
            {
                ecs_set_target_fps(world, wls->target_fps);
                rls->target_fps = wls->target_fps;
            }
            return result::ok;
        }

        [[nodiscard]] result update(const uint32_t viewport_width, const uint32_t viewport_height,
                                    const double dt) const
        {
            camera* cam = active_cam == level_state::camera_choice::game ? game_cam : free_cam;
            if (const auto res = cam->update(viewport_width, viewport_height, dt); res != result::ok)
            {
                return res;
            }
            ecs_progress(world, static_cast<float>(dt));
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
            else if (active_cam == camera_choice::game)
            {
                if (event.type == SDL_EVENT_MOUSE_WHEEL)
                {
                    rls->game_camera_yaw = wls->game_camera_yaw;
                    const auto mbe = reinterpret_cast<const SDL_MouseWheelEvent&>(event);
                    float direction = mbe.direction == SDL_MOUSEWHEEL_NORMAL ? 1.f : -1.f;
                    float new_yaw = rls->game_camera_yaw + mbe.y / 25.f * direction;
                    if (new_yaw < 0.f) new_yaw += glm::pi<float>() * 2;
                    if (new_yaw >= glm::pi<float>() * 2) new_yaw -= glm::pi<float>() * 2;
                    const std::array<float, 4> pos = rosy_reference.node->position;
                    game_cam->set_yaw_around_position(new_yaw, {pos[0], pos[1], pos[2]});
                }
            }

            constexpr Uint8 rosy_attention_btn{1};
            constexpr Uint8 pick_debug_toggle_btn{2};
            constexpr Uint8 pick_debug_record_btn{3};
            const ecs_entity_t rosy_entity = rosy_reference.entity;
            if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type ==
                SDL_EVENT_MOUSE_BUTTON_UP)
            {
                const auto mbe = reinterpret_cast<const SDL_MouseButtonEvent&>(event);
                if (std::isnan(mbe.x) || (std::isnan(mbe.y))) return result::ok;
                const c_cursor_position c_pos{.screen_x = mbe.x, .screen_y = mbe.y};
                ecs_set_id(world, level_entity, ecs_id(c_cursor_position), sizeof(c_cursor_position), &c_pos);
            }
            if (rosy_reference.node != nullptr && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                switch (const auto mbe = reinterpret_cast<const SDL_MouseButtonEvent&>(event); mbe.button)
                {
                case rosy_attention_btn:
                    ecs_add(world, rosy_entity, t_rosy_action);
                    break;
                case pick_debug_toggle_btn:
                    if (mbe.clicks == 1 && ecs_has_id(world, level_entity, ecs_id(c_pick_debugging_enabled)))
                    {
                        ecs_remove(world, level_entity, c_pick_debugging_enabled);
                        ecs_add(world, level_entity, t_pick_debugging_clear);
                    }
                    else
                    {
                        const uint32_t space = mbe.clicks > 2
                                                   ? debug_object_flag_view_space
                                                   : debug_object_flag_screen_space;
                        const c_pick_debugging_enabled pick{.space = space};
                        ecs_set_id(world, level_entity, ecs_id(c_pick_debugging_enabled),
                                   sizeof(c_pick_debugging_enabled), &pick);
                    }
                    break;
                case pick_debug_record_btn:
                    ecs_add(world, level_entity, t_pick_debugging_record);
                    break;
                default:
                    break;
                }
            }
            if (rosy_reference.node != nullptr && event.type == SDL_EVENT_MOUSE_BUTTON_UP)
            {
                if (const auto mbe = reinterpret_cast<const SDL_MouseButtonEvent&>(event); mbe.button ==
                    rosy_attention_btn)
                    ecs_remove(world, rosy_entity, t_rosy_action);
            }
            return result::ok;
        }
    };

    level_state* ls{nullptr};
    scene_graph_processor* sgp{nullptr};

    // **** ECS SYSTEM DEFINITIONS ****/

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void move_rosy(ecs_iter_t* it)
    {
        // This function moves rosy to where the screen cursor is when the left mouse button is pushed.
        const auto ctx = static_cast<level_state*>(it->param);
        for (int i = 0; i < it->count; i++)
        {
            if (!ecs_has_id(ctx->world, ctx->level_entity, ecs_id(c_cursor_position))) continue;

            // Start picking. Picking here works by converting the 2D screen space cursor position to Vulkan NDC coordinates and then converts them
            // to clip space. From clip space their position is on the projection plane in view space. A ray is created from the click position on the projection plane from
            // the view space camera position, which is just origin. The ray is transformed to world space, the floor's origin is the same as the world space origin.
            // Plucker coordinates are used to calculate the ray's intersection in the floor plane, as derived from Foundations of Game Engine Development [Lengyel]
            const auto c_pos = static_cast<const c_cursor_position*>(ecs_get_id(
                ctx->world, ctx->level_entity, ecs_id(c_cursor_position)));
            const camera* cam = ctx->active_cam == level_state::camera_choice::game ? ctx->game_cam : ctx->free_cam;

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

            ctx->l->debug(std::format("intersection {:.3f}, {:.3f}, {:.3f}", intersection[0] / intersection_w,
                                      intersection[1] / intersection_w, intersection[2] / intersection_w));

            // Set rosy to target that intersection.
            auto rosy_target = glm::vec3(intersection[0] / intersection_w, intersection[1] / intersection_w,
                                         intersection[2] / intersection_w);

            // Calculate whether the target is within the floor's bounds, if not set any coordinate outside to the max extent of the bounds.
            const auto floor_index = static_cast<const c_static*>(ecs_get_id(
                ctx->world, ctx->floor_entity, ecs_id(c_static)));
            const node* floor_node = ctx->get_static()[floor_index->index];
            // Transform world space rosy target to the actual floor meshes object space because that's the space the bounds are in.
            auto min_bounds = glm::vec3(array_to_mat4(floor_node->transform) * glm::vec4(
                floor_node->bounds.min[0], floor_node->bounds.min[1], floor_node->bounds.min[2], 1.f));
            auto max_bounds = glm::vec3(array_to_mat4(floor_node->transform) * glm::vec4(
                floor_node->bounds.max[0], floor_node->bounds.max[1], floor_node->bounds.max[2], 1.f));
            for (int j{0}; j < 3; j++)
            {
                if (rosy_target[j] < min_bounds[j])
                {
                    rosy_target[j] = min_bounds[j];
                }
                if (rosy_target[j] > max_bounds[j])
                {
                    rosy_target[j] = max_bounds[j];
                }
            }

            c_target target{.x = rosy_target.x, .y = rosy_target.y, .z = rosy_target.z};
            ecs_set_id(ctx->world, ctx->rosy_reference.entity, ecs_id(c_target), sizeof(c_target), &target);

            // Draw some debugging UI to display picking performance.
            if (ecs_has_id(ctx->world, ctx->level_entity, ecs_id(c_pick_debugging_enabled)))
            {
                const auto pick_debugging = static_cast<const c_pick_debugging_enabled*>(ecs_get_id(
                    ctx->world, ctx->level_entity, ecs_id(c_pick_debugging_enabled)));
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
                ctx->rls->pick_debugging.picking = {
                    .type = debug_object_type::circle,
                    .transform = mat4_to_array(m),
                    .color = color,
                    .flags = pick_debugging->space,
                };
                // Record the ray as a debug line to display:
                if (ecs_has_id(ctx->world, ctx->level_entity, ecs_id(t_pick_debugging_record)))
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
                    ctx->rls->pick_debugging.circles.push_back({
                        .type = debug_object_type::line,
                        .transform = mat4_to_array(m2),
                        .color = {0.f, 1.f, 0.f, 1.f},
                        .flags = debug_object_flag_transform_is_points,
                    });
                    ecs_remove(ctx->world, ctx->level_entity, t_pick_debugging_record);
                }
            }
            else
            {
                // In this case, if we're in edit mode we will draw nice little cursor on the actual floor where rosy is targeting.
                glm::mat4 circle_m = glm::translate(glm::mat4(1.f),
                                                    glm::vec3(intersection[0] / intersection_w,
                                                              (intersection[1] / intersection_w) + 0.1f,
                                                              intersection[2] / intersection_w));
                circle_m = glm::rotate(circle_m, (glm::pi<float>() / 2.f), glm::vec3(1.f, 0.f, 0.f));
                circle_m = glm::scale(circle_m, glm::vec3(0.25f));

                ctx->rls->pick_debugging.picking = {
                    .type = debug_object_type::circle,
                    .transform = mat4_to_array(circle_m),
                    .color = {0.f, 1.f, 0.f, 1.f},
                    .flags = 0,
                };
            }
            // If rosy is targeting orient rosy and move her toward the target.
            if (ecs_has_id(ctx->world, ctx->rosy_reference.entity, ecs_id(c_target)))
            {
                const auto rosy_position = ctx->rosy_reference.node->position;
                const auto rosy_pos = glm::vec3(rosy_position[0], rosy_position[1], rosy_position[2]);
                const glm::mat4 rosy_translate = glm::translate(glm::mat4(1.f), rosy_pos);
                constexpr auto game_forward = glm::vec3(0.f, 0.f, 1.f);
                const glm::mat4 to_rosy_space = glm::inverse(rosy_translate);

                // convert rosy target to rosy's object space and verify that the rosy_space target isn't the null vector, if it is she's already there.
                // This works because rosy will be at origin in this space and so wherever the target is at an angle for rosy's position.
                if (const auto rosy_space_target = glm::vec3(to_rosy_space * glm::vec4(rosy_target, 1.f));
                    rosy_space_target != glm::zero<glm::vec3>())
                {
                    // Calculate the difference between rosy's orientation and her target position
                    const float offset = rosy_space_target.x > 0 ? -1.f : 0.f;
                    // This offset's rosy's orientation based on her orientation around the x-axis.
                    const float target_game_cos_theta = glm::dot(glm::normalize(rosy_space_target), game_forward);
                    // This dot product is used to get the angle of the target with respect to rosy's position.
                    const float target_yaw = -std::acos(target_game_cos_theta);
                    // This is negated because we of the handedness of the coordinate system.
                    const float new_yaw = target_yaw + offset;

                    ctx->l->debug(std::format(
                        "(target: ({:.3f}, {:.3f}), {:.3f})) cosTheta {:.3f}) yaw: {:.3f}) offset: {:.3f} new_yaw: {:.3f}",
                        rosy_space_target[0],
                        rosy_space_target[1],
                        rosy_space_target[2],
                        target_game_cos_theta,
                        target_yaw,
                        offset,
                        new_yaw
                    ));

                    c_forward nf{.yaw = target_yaw};
                    ecs_set_id(ctx->world, ctx->rosy_reference.entity, ecs_id(c_forward), sizeof(c_forward), &nf);

                    // Set rosy's orientation to face target
                    glm::quat yaw_rotation = angleAxis(target_yaw, glm::vec3{0.f, 1.f, 0.f});
                    if (offset < 0.f) yaw_rotation = glm::inverse(yaw_rotation);
                    const glm::mat4 r = toMat4(yaw_rotation);

                    // Linearly interpolate rosy's position toward the target
                    const float t = 1.f * it->delta_time;
                    glm::vec3 new_rosy_pos = (rosy_pos * (1.f - t)) + rosy_target * t;

                    // Update rosy's transform.
                    const glm::mat4 tr = glm::translate(glm::mat4(1.f), new_rosy_pos);
                    if (const auto res = ctx->rosy_reference.node->update_transform(mat4_to_array(tr * r)); res !=
                        result::ok)
                    {
                        ctx->l->error("error transforming rosy");
                    }

                    ctx->game_cam->set_game_cam_position({new_rosy_pos[0], new_rosy_pos[1], new_rosy_pos[2]});
                }
            }
        }
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void init_level_state(ecs_iter_t* it)
    {
        const auto ctx = static_cast<level_state*>(it->param);
        {
            // Write active camera values
            if (std::abs(ctx->game_cam->yaw - ctx->wls->game_camera_yaw) >= 0.2)
            {
                const std::array<float, 4> pos = ctx->rosy_reference.node->position;
                ctx->game_cam->set_yaw_around_position(ctx->wls->game_camera_yaw, {pos[0], pos[1], pos[2]});
            }
            const camera* cam = ctx->active_cam == level_state::camera_choice::game ? ctx->game_cam : ctx->free_cam;
            ctx->rls->cam.p = cam->p;
            ctx->rls->cam.v = cam->v;
            ctx->rls->cam.vp = cam->vp;
            ctx->rls->cam.position = cam->position;
            ctx->rls->cam.pitch = cam->pitch;
            ctx->rls->cam.yaw = cam->yaw;
            ctx->rls->game_camera_yaw = ctx->game_cam->yaw;
        }
        {
            // Configure draw options based on writable level state
            ctx->rls->debug_enabled = ctx->wls->enable_edit;
            ctx->rls->draw_config = ctx->wls->draw_config;
            ctx->rls->light = ctx->wls->light;
        }
        {
            // Fragment config
            ctx->rls->fragment_config = ctx->wls->fragment_config;
        }
        {
            // Mob state
            const std::vector<node*> mobs = ctx->get_mobs();
            if (ctx->wls->mob_edit.submitted)
            {
                ctx->rls->mob_read.clear_edits = true;
                if (mobs.size() > ctx->wls->mob_edit.edit_index)
                {
                    if (const auto res = mobs[0]->set_position(ctx->wls->mob_edit.position); res != result::ok)
                    {
                        ctx->l->error("Error updating mob position");
                    }
                }
            }
            else
            {
                ctx->rls->mob_read.clear_edits = false;
            }
            ctx->rls->mob_read.mob_states.clear();
            for (const game_node_reference& nr : ctx->game_nodes)
            {
                std::array<float, 3> target = {0.f, 0.f, 0.f};
                float yaw = 0.f;
                if (ecs_has_id(ctx->world, nr.entity, ecs_id(c_target)))
                {
                    const auto tc = static_cast<const c_target*>(ecs_get_id(ctx->world, nr.entity, ecs_id(c_target)));
                    target = {tc->x, tc->y, tc->z};
                }
                if (ecs_has_id(ctx->world, nr.entity, ecs_id(c_forward)))
                {
                    const auto fc = static_cast<const c_forward*>(ecs_get_id(ctx->world, nr.entity, ecs_id(c_forward)));
                    ctx->l->debug(std::format("setting rls yaw: ({:.3f}", fc->yaw));
                    yaw = fc->yaw;
                }
                ctx->rls->mob_read.mob_states.push_back({
                    .name = nr.node->name,
                    .position = {nr.node->position[0], nr.node->position[1], nr.node->position[2]},
                    .yaw = yaw,
                    .target = target,
                });
            }
        }
        ctx->rls->debug_objects.clear();
        if (ecs_has_id(ctx->world, ctx->level_entity, ecs_id(t_pick_debugging_clear)))
        {
            ctx->rls->pick_debugging.circles.clear();
            ecs_remove(ctx->world, ctx->level_entity, t_pick_debugging_clear);
        }
        if (ecs_has_id(ctx->world, ctx->level_entity, ecs_id(c_pick_debugging_enabled)))
        {
            const auto pick_debugging = static_cast<const c_pick_debugging_enabled*>(ecs_get_id(
                ctx->world, ctx->level_entity, ecs_id(c_pick_debugging_enabled)));
            ctx->rls->debug_objects.insert(ctx->rls->debug_objects.end(), ctx->rls->pick_debugging.circles.begin(),
                                           ctx->rls->pick_debugging.circles.end());
            if (ctx->rls->pick_debugging.picking.has_value())
            {
                ctx->rls->debug_objects.push_back(ctx->rls->pick_debugging.picking.value());
                ctx->rls->pick_debugging.picking = std::nullopt;
            }
            ctx->rls->pick_debugging.space = pick_debugging->space & debug_object_flag_screen_space
                                                 ? pick_debug_read_state::picking_space::screen
                                                 : pick_debug_read_state::picking_space::view;
        }
        else if (ctx->rosy_reference.node != nullptr && ecs_has_id(ctx->world, ctx->rosy_reference.entity,
                                                                   ecs_id(t_rosy_action)))
        {
            if (ctx->rls->pick_debugging.picking.has_value())
            {
                ctx->rls->debug_objects.push_back(ctx->rls->pick_debugging.picking.value());
            }
        }
        else
        {
            ctx->rls->pick_debugging.space = pick_debug_read_state::picking_space::disabled;
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
                    const glm::mat4 light_translate = glm::translate(glm::mat4(1.f), {
                                                                         0.f, 0.f,
                                                                         1.f * ctx->wls->light_debug.sun_distance
                                                                     });
                    debug_light_translate = glm::translate(glm::mat4(1.f), {
                                                               0.f, 0.f, -1.f * ctx->wls->light_debug.sun_distance
                                                           });
                    const glm::quat pitch_rotation = angleAxis(-ctx->wls->light_debug.sun_pitch,
                                                               glm::vec3{1.f, 0.f, 0.f});
                    const glm::quat yaw_rotation = angleAxis(ctx->wls->light_debug.sun_yaw, glm::vec3{0.f, -1.f, 0.f});
                    light_line_rot = toMat4(yaw_rotation) * toMat4(pitch_rotation);

                    const auto camera_position = glm::vec3(
                        light_line_rot * glm::vec4(0.f, 0.f, -ctx->wls->light_debug.sun_distance, 0.f));
                    auto sunlight = glm::vec4(glm::normalize(camera_position), 1.f);
                    light_sun_view = light_line_rot * light_translate;
                    debug_light_sun_view = light_line_rot * (ctx->wls->light_debug.enable_light_perspective
                                                                 ? light_translate
                                                                 : debug_light_translate);

                    ctx->rls->light.sunlight = {sunlight[0], sunlight[1], sunlight[2], sunlight[3]};
                };
            }

            if (ctx->wls->light_debug.enable_sun_debug)
            {
                // Generate debug lines for light and shadow debugging
                const glm::mat4 debug_draw_view = light_line_rot * debug_light_translate;
                const glm::mat4 debug_light_line = glm::scale(debug_draw_view, {
                                                                  ctx->wls->light_debug.sun_distance,
                                                                  ctx->wls->light_debug.sun_distance,
                                                                  ctx->wls->light_debug.sun_distance
                                                              });

                debug_object line;
                line.type = debug_object_type::line;
                line.transform = mat4_to_array(debug_light_line);
                line.color = {1.f, 0.f, 0.f, 1.f};
                if (ctx->wls->light_debug.enable_sun_debug) ctx->rls->debug_objects.push_back(line);
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
                        ctx->rls->debug_objects.push_back(sun_circle);
                    }
                }
            }

            glm::mat4 cam_lv;
            glm::mat4 cam_lp;
            {
                // Create Light view and projection

                const float cascade_level = ctx->wls->light_debug.cascade_level;
                auto light_projections = glm::mat4(
                    glm::vec4(2.f / cascade_level, 0.f, 0.f, 0.f),
                    glm::vec4(0.f, -2.f / cascade_level, 0.f, 0.f),
                    glm::vec4(0.f, 0.f, -1.f / ctx->wls->light_debug.orthographic_depth, 0.f),
                    glm::vec4(0.f, 0.f, 0.f, 1.f)
                );

                const glm::mat4 lv = light_sun_view;
                const glm::mat4 lp = light_projections;
                cam_lv = glm::inverse(debug_light_sun_view);
                cam_lp = ctx->wls->light_debug.enable_light_perspective
                             ? light_projections
                             : array_to_mat4((ctx->rls->cam.p));
                ctx->rls->cam.shadow_projection_near = mat4_to_array(lp * glm::inverse(lv));
            }

            if (ctx->wls->light_debug.enable_light_cam)
            {
                // Set debug lighting options on
                ctx->rls->debug_enabled = false;
                ctx->rls->cam.v = mat4_to_array(cam_lv);
                ctx->rls->cam.vp = mat4_to_array(cam_lp * cam_lv);
            }
        }
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void detect_mob(ecs_iter_t* it)
    {
        const auto ctx = static_cast<level_state*>(it->param);
        const c_mob* p = ecs_field(it, c_mob, 0);

        for (int i = 0; i < it->count; i++)
        {
            const ecs_entity_t e = it->entities[i];

            const game_node_reference ref = ctx->game_nodes[p[i].index];
            ctx->l->debug(std::format("mob detected @ index {} with name: '{}'", static_cast<int>(p[i].index),
                                      ref.node->name));
            if (ecs_has_id(ctx->world, e, ecs_id(t_rosy)))
            {
                ctx->l->debug(std::format("Rosy detected!"));
            }
        }
    }

    // ReSharper disable once CppParameterMayBeConstPtrOrRef
    void detect_floor(ecs_iter_t* it)
    {
        const auto ctx = static_cast<level_state*>(it->param);

        for (int i = 0; i < it->count; i++)
        {
            ctx->l->debug(std::format("floor detected"));
        }
    }
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
            wls.light.depth_bias_enabled = true;
            wls.draw_config.thick_wire_lines = false;
            wls.fragment_config.output = 0;
            wls.fragment_config.light_enabled = true;
            wls.fragment_config.tangent_space_enabled = true;
            wls.fragment_config.shadows_enabled = true;
            wls.target_fps = initial_fps_target;
            rls.ui_enabled = true;
        }
        ls = new(std::nothrow) level_state;
        if (ls == nullptr)
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

    const auto& scene = new_asset.scenes[root_scene_index];
    if (scene.nodes.empty()) return result::invalid_argument;

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
    for (const auto& node_index : scene.nodes)
    {
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
            new_game_node->deinit();
            delete new_game_node;
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
    size_t go_mob_index{0};
    size_t go_static_index{0};
    while (sgp->queue.size() > 0)
    {
        // ReSharper disable once CppUseStructuredBinding Visual studio wants to make this a reference, and it shouldn't be.
        stack_item queue_item = sgp->queue.front();
        sgp->queue.pop();
        assert(queue_item.game_node != nullptr);

        glm::mat4 node_transform = array_to_mat4(queue_item.stack_node.transform);
        const glm::mat4 transform = queue_item.parent_transform * node_transform;
        // Set node bounds for bound testing.
        node_bounds bounds{};
        // Advance the next item in the node queue
        if (queue_item.stack_node.mesh_id < new_asset.meshes.size())
        {
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
                    for (const auto& surf : current_mesh.surfaces)
                    {
                        for (size_t i{0}; i < 3; i++)
                        {
                            bounds.min[i] = glm::min(surf.min_bounds[i], bounds.min[i]);
                            bounds.max[i] = glm::max(surf.max_bounds[i], bounds.max[i]);
                        }
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
                        if (new_asset.materials.size() > surf.material && new_asset.materials[surf.material].alpha_mode
                            != 0)
                        {
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
        queue_item.game_node->bounds = bounds;

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
            if (const auto res = new_game_node->init(ls->l, mat4_to_array(transform), mat4_to_array(node_transform));
                res != result::ok)
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
            for (size_t i{0}; i < mobs.size(); i++)
            {
                node* n = mobs[i];
                ecs_entity_t node_entity = ecs_new(ls->world);

                game_node_reference ref = {
                    .entity = node_entity,
                    .index = i,
                    .node = n,
                };
                ls->game_nodes[i] = ref;

                c_mob m{i};
                ecs_set_id(ls->world, node_entity, ecs_id(c_mob), sizeof(c_mob), &m);
                c_forward forward{.yaw = 0.f};
                ecs_set_id(ls->world, node_entity, ecs_id(c_forward), sizeof(c_forward), &forward);

                if (n->name == "rosy")
                {
                    ls->rosy_reference = ref;
                    ecs_add(ls->world, node_entity, t_rosy);
                    const std::array<float, 4> pos = ls->rosy_reference.node->position;
                    ls->game_cam->set_game_cam_position({pos[0], pos[1], pos[2]});
                }
            }
        }
        {
            // Track special static objects
            std::vector<node*> static_objects = ls->get_static();
            for (size_t i{0}; i < static_objects.size(); i++)
            {
                node* n = static_objects[i];
                ecs_entity_t node_entity = ecs_new(ls->world);

                if (n->name == "floor")
                {
                    ls->floor_entity = node_entity;
                    ecs_add(ls->world, node_entity, t_floor);
                    c_static m{i};
                    ecs_set_id(ls->world, node_entity, ecs_id(c_static), sizeof(c_static), &m);
                    break;
                }
            }
        }
    }
    return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::setup_frame()
{
    graphics_object_update_data.graphic_objects.clear();
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

result level::process()
{
    graphics_object_update_data.offset = static_objects_offset;
    graphics_object_update_data.graphic_objects.resize(num_dynamic_objects);

    for (const std::vector<node*> mobs = ls->get_mobs(); const node* n : mobs) n->populate_graph(
        graphics_object_update_data.graphic_objects);
    return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result level::process_sdl_event(const SDL_Event& event)
{
    return ls->process_sdl_event(event);
}
