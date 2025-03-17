#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>

// These are type declarations, not default configurations. Configure those in Level.cpp or elsewhere.
namespace rosy
{
    enum class result : uint8_t
    {
        ok,
        state_changed,
        error,
        invalid_argument,
        allocation_failure,
        graphics_init_failure,
        graphics_frame_failure,
        graphics_swapchain_failure,
        open_failed,
        write_failed,
        copy_failed,
        read_failed,
        create_failed,
        update_failed,
        overflow,
        invalid_state,
    };

    struct config
    {
        int max_window_width = 0;
        int max_window_height = 0;
    };

    struct surface_graphics_data
    {
        size_t mesh_index{0};
        size_t graphic_objects_offset{0};
        size_t graphics_object_index{0};
        size_t material_index{0};
        uint32_t index_count{0};
        uint32_t start_index{0};
        bool blended{false};
    };

    struct graphics_object
    {
        size_t index{0};
        std::vector<surface_graphics_data> surface_data{};
        std::array<float, 16> transform{};
        std::array<float, 16> to_object_space_transform{};
        std::array<float, 9> normal_transform{};
    };

    struct graphics_object_update
    {
        size_t offset{0};
        std::vector<graphics_object> graphic_objects{};
        std::vector<graphics_object> full_scene{};
    };

    struct engine_stats
    {
        float a_fps{0.f};
        float d_fps{0.f};
        float r_fps{0.f};
        float frame_time{0.f};
        float level_update_time{0.f};
    };

    enum class debug_object_type : uint8_t
    {
        line,
        circle,
        cross,
    };

    inline uint32_t debug_object_flag_screen_space{1 << 0};
    inline uint32_t debug_object_flag_view_space{1 << 1};
    inline uint32_t debug_object_flag_transform_is_points{1 << 2};

    struct debug_object
    {
        debug_object_type type{debug_object_type::line};
        std::array<float, 16> transform{};
        std::array<float, 4> color{};
        uint32_t flags{0};
    };

    struct read_camera
    {
        std::array<float, 16> v{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        std::array<float, 16> p{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        std::array<float, 16> vp{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        std::array<float, 16> shadow_projection_near{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        std::array<float, 4> position{};
        float pitch{0.f};
        float yaw{0.f};
    };

    struct light_read_write_state
    {
        std::array<float, 4> sun_position{};
        std::array<float, 4> sunlight_color{};
        bool depth_bias_enabled{false};
        float ambient_light{0.f};
        float depth_bias_constant{0.f};
        float depth_bias_clamp{0.f};
        float depth_bias_slope_factor{0.f};
        bool flip_light_x{false};
        bool flip_light_y{false};
        bool flip_light_z{false};
        bool flip_tangent_x{false};
        bool flip_tangent_y{false};
        bool flip_tangent_z{false};
        bool flip_tangent_w{false};
        bool inverse_bnt{false};
        bool ignore_asset_tangent_sign{false};
        bool ensure_orthogonal_bitangent{false};
        bool brdf_lighting_enabled{false};
    };

    struct light_debug_state
    {
        float sun_distance{0};
        float sun_pitch{0};
        float sun_yaw{0};
        float cascade_level{0};
        bool enable_light_cam{false};
        bool enable_sun_debug{false};
        bool enable_light_perspective{false};
        float orthographic_depth{0};
    };

    struct draw_config_state
    {
        bool reverse_winding_order_enabled{false};
        bool cull_enabled{false};
        bool wire_enabled{false};
        bool thick_wire_lines{false};
    };

    struct fragment_config_state
    {
        // output enables debug color output in the fragment shader.
        // 0 default
        // 1 normals
        // 2 tangent
        // 3 light
        // 4 view
        // 5 vertex colors
        // 6 normal map
        // 7 bitangent
        // 8 cosTheta
        // 9 cosPhi
        int output{0};
        bool light_enabled{false};
        bool tangent_space_enabled{false};
        bool shadows_enabled{false};
        bool normal_maps_enabled{false};
    };

    struct mob_state
    {
        std::string name;
        std::array<float, 3> position;
        float yaw{0.f};
        std::array<float, 3> target{0.f, 0.f, 0.f};
    };

    struct mob_edit_state
    {
        size_t edit_index{0};
        std::array<float, 3> position{0.f, 0.f, 0.f};
        bool submitted{false};
        bool updated{false};
    };

    struct mob_read_state
    {
        bool clear_edits{false};
        std::vector<mob_state> mob_states{};
    };

    struct graphic_objects_state
    {
        size_t static_objects_offset{0};
    };

    struct pick_debug_read_state
    {
        enum class picking_space: uint8_t
        {
            disabled,
            screen,
            view,
        };

        picking_space space{picking_space::disabled};
        std::optional<debug_object> picking{std::nullopt};
        std::vector<debug_object> circles;
    };

    struct editor_command_node_data
    {
        std::array<float, 3> location{};
        float scale{1.f};
        float yaw{0.f};
    };

    struct editor_command
    {
        enum class editor_command_type : uint8_t
        {
            no_command,
            load_asset,
            write_level,
            read_level,
            add_to_level,
            remove_from_level,
            edit_level_node,
        };

        enum class model_type : uint8_t
        {
            no_model,
            mob_model,
            static_model,
        };

        editor_command_type command_type{editor_command_type::no_command};
        model_type mode_type_option{model_type::no_model};
        std::string id{};
        editor_command_node_data node_data{};
    };

    struct level_data_model
    {
        std::string id{};
        std::string name{};
        std::array<float, 3> location{};
        float scale{1.f};
        float yaw{0.f};
        editor_command::model_type model_type{editor_command::model_type::no_model};
    };

    struct level_data
    {
        std::vector<level_data_model> mob_models;
        std::vector<level_data_model> static_models;
    };

    struct level_editor_commands
    {
        std::vector<editor_command> commands;
    };

    struct model_description
    {
        std::string id{};
        std::string name{};
        std::array<float, 3> location{};
        float scale{1.f};
        float yaw{0.f};
    };

    struct asset_description
    {
        std::string id{};
        std::string name{};
        std::vector<model_description> models;
        const void* asset{nullptr};
    };

    struct level_editor_state
    {
        std::vector<asset_description> assets;
        const void* new_asset{nullptr};
        level_data current_level_data{};
    };

    struct read_level_state
    {
        float target_fps{0.f};
        bool debug_enabled{false};
        bool ui_enabled{false};
        bool cursor_enabled{true};

        read_camera cam{};

        light_read_write_state light{};
        draw_config_state draw_config{};
        std::vector<debug_object> debug_objects{};
        fragment_config_state fragment_config{};
        graphic_objects_state graphic_objects{};
        mob_read_state mob_read{};
        pick_debug_read_state pick_debugging{};
        level_editor_state editor_state{};
        graphics_object_update go_update{};

        float game_camera_yaw{0};
    };

    struct write_level_state
    {
        bool enable_edit{false};
        light_read_write_state light{};
        light_debug_state light_debug{};
        draw_config_state draw_config{};
        fragment_config_state fragment_config{};
        mob_edit_state mob_edit{};
        level_editor_commands editor_commands{};
        float target_fps{0.f};
        float game_camera_yaw{0};
    };
}
