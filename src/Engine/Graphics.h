#pragma once
#include "Types.h"
#include "Telemetry.h"
#include "../Packager/Asset.h"

// ReSharper disable once CppInconsistentNaming
using SDL_Window = struct SDL_Window;


namespace rosy
{
    struct graphics
    {
        const log* l{nullptr};
        uint32_t viewport_width{0};
        uint32_t viewport_height{0};

        [[nodiscard]] result init(SDL_Window* new_window, const log* new_log, config cfg);
        [[nodiscard]] result set_asset(const rosy_packager::asset& a,
                                       const std::vector<graphics_object>& graphics_objects,
                                       write_level_state* wls) const;
        [[nodiscard]] result update(const read_level_state& rls, const graphics_object_update& graphics_objects_update);
        [[nodiscard]] result render(const engine_stats& stats);
        [[nodiscard]] result resize();
        void deinit();
    };
}
