#pragma once
#include "imgui.h"
#include "Types.h"

namespace rosy
{
    struct graphics_stats
    {
        int triangle_count{0};
        int line_count{0};
        int draw_call_count{0};
        float draw_time{0.f};
    };

    struct graphics_data
    {
        std::array<float, 4> sun_position{};
        std::array<float, 4> camera_position{};
        ImTextureID shadow_map_img_id{};
    };

    struct debug_ui
    {
        write_level_state* wls{};
        size_t selected_asset{0};
        size_t selected_model{0};
        bool asset_details{true};
        bool model_details{true};
        std::array<char, saved_view_name_size> view_name{};

        std::array<float, 3> level_edit_translate{};
        float level_edit_scale{1.f};
        float level_edit_yaw{0.f};
        editor_command::model_type level_edit_model_type{editor_command::model_type::no_model};
        std::string level_edit_model_id{};

        void graphics_debug_ui(const engine_stats& eng_stats, const graphics_stats& stats, const graphics_data& data,
                               const read_level_state* rls) const;
        void assets_debug_ui(const read_level_state* rls);
        void saved_views_debug_ui(const read_level_state* rls);
    };
}
