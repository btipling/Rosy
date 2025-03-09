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
        std::shared_ptr<log> l{nullptr};
        uint32_t viewport_width{0};
        uint32_t viewport_height{0};

        [[nodiscard]] result init(SDL_Window* new_window, const std::shared_ptr<log>& new_log, config cfg);
        [[nodiscard]] result update(const read_level_state& rls, write_level_state* wls) const;
        [[nodiscard]] result render(const engine_stats& stats);
        [[nodiscard]] result resize();
        void deinit();
    };
}
