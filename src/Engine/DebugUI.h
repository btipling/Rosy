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
        std::array<float, 4> sunlight{};
        std::array<float, 4> camera_position{};
        ImTextureID shadow_mage_img_id{};
    };

    struct debug_ui
    {
        write_level_state* wls{};
        size_t selected_asset{0};

        void graphics_debug_ui(const engine_stats& eng_stats, const graphics_stats& stats, const graphics_data& data,
                               const read_level_state* rls) const;
        void assets_debug_ui(const read_level_state* rls);
    };
}
