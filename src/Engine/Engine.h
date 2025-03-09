#pragma once
#include "Types.h"
#include "Telemetry.h"
#include "Graphics.h"
#include "Level.h"


// ReSharper disable once CppInconsistentNaming
using SDL_Window = struct SDL_Window;

namespace rosy
{
    struct engine
    {
        // Owned resources
        std::shared_ptr<log> l{nullptr};
        SDL_Window* window{nullptr};
        level* lvl{nullptr};
        graphics* gfx{nullptr};

        // Timing
        uint64_t start_time{0};
        uint64_t last_frame_time{0};

        // Profiling
        engine_stats stats{};

        [[nodiscard]] result init();
        [[nodiscard]] result run();
        [[nodiscard]] result run_frame();
        void deinit();
    };
}
