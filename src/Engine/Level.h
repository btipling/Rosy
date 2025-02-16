#pragma once
#include "Types.h"
#include "Telemetry.h"
#include <SDL3/SDL.h>

namespace rosy
{
    struct level
    {
        read_level_state rls{};
        write_level_state wls{};

        result init(log* new_log, [[maybe_unused]] config new_cfg);
        void deinit();
        result setup_frame();
        result update(const uint32_t viewport_width, const uint32_t viewport_height, double dt);
        result process();
        result process_sdl_event(const SDL_Event& event);
    };
}
