#include "pch.h"
#include "Engine.h"

#include <thread>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include <tracy/Tracy.hpp>
#ifdef TRACY_ENABLED
#include <client/TracyProfiler.hpp>
#endif

constexpr uint64_t sdl_time_to_seconds{1'000'000'000};

using namespace rosy;

//// Engine

// ReSharper disable once CppParameterMayBeConstPtrOrRef
static bool event_handler(void* userdata, SDL_Event* event)  // NOLINT(misc-use-anonymous-namespace)
{
    // NOLINT(misc-use-anonymous-namespace)
    const auto eng = static_cast<engine*>(userdata);
    // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
    switch (event->type)
    {
    case SDL_EVENT_WINDOW_RESIZED:
        if (result res = eng->gfx->resize(); res != result::ok)
        {
            eng->l->error(std::format("resizing-event: gfx failed to resize swapchain {}\n",
                                      static_cast<uint8_t>(res)));
            return false;
        }
        if (result res = eng->run_frame(); res != result::ok)
        {
            eng->l->error(std::format("resizing-event: gfx failed to render {}\n", static_cast<uint8_t>(res)));
            return false;
        }
        break;
    }
    return true;
}

result engine::init()
{
    try { l = std::make_shared<log>(); }
    catch (const std::bad_alloc&) {
        return result::allocation_failure;
    }
#ifdef ROSY_LOG_LEVEL_DEBUG
	l->level = rosy::log_level::debug;
#endif

    l->info("Engine init begin");

    SDL_Time tick;
    if (!SDL_GetCurrentTime(&tick))
    {
        l->error(std::format("Failed to get SDL time: {}", SDL_GetError()));
        return result::error;
    }
    start_time = tick;

    config cfg;
    // SDL Window initialization.
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            l->error(std::format("SDL initialization failed: {}", SDL_GetError()));
            return result::error;
        }

        int width;
        int height;
        int displays_count = 0;
        const auto display_ids = SDL_GetDisplays(&displays_count);
        if (!display_ids || displays_count <= 0)
        {
            l->error(std::format("Failed to get SDL display info: {}", SDL_GetError()));
            return result::error;
        }

        SDL_Rect display_bounds = {};
        if (SDL_GetDisplayBounds(*display_ids, &display_bounds))
        {
            l->debug("Got display bounds");
            cfg.max_window_width = display_bounds.w;
            cfg.max_window_height = display_bounds.h;
            width = static_cast<int>(std::floor(static_cast<float>(display_bounds.w) * 0.75));
            height = static_cast<int>(std::floor(static_cast<float>(display_bounds.h) * 0.75));
        }
        else
        {
            l->error(std::format("SDL getting display bounds failed: {}", SDL_GetError()));
            return result::error;
        }

        constexpr SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        window = SDL_CreateWindow("Rosy", width, height, window_flags);
        if (!window)
        {
            l->error(std::format("Window creation failed: {}", SDL_GetError()));
            return result::error;
        }

        if (!SDL_AddEventWatch(event_handler, static_cast<void*>(this)))
        {
            l->error(std::format("Failed to add event watcher: {}", SDL_GetError()));
            return result::error;
        }
    }

    // Level initialization
    {
        if (lvl = new(std::nothrow) level{}; lvl == nullptr)
        {
            l->error("Error allocating level");
            return result::allocation_failure;
        }
        if (const auto res = lvl->init(l, cfg); res != result::ok)
        {
            l->error(std::format("Level creation failed: {}", static_cast<uint8_t>(res)));
            return res;
        }
    }

    // Graphics engine initialization
    {
        if (gfx = new(std::nothrow) graphics{}; gfx == nullptr)
        {
            l->error("Error allocating graphics engine");
            return result::allocation_failure;
        }
        if (const auto res = gfx->init(window, l, cfg); res != result::ok)
        {
            l->error(std::format("Graphics creation failed: {}", static_cast<uint8_t>(res)));
            return res;
        }
    }

    l->info("Engine init done");
    return result::ok;
}

void engine::deinit()
{
    if (l) l->info("Engine deinit start");

    if (gfx)
    {
        gfx->deinit();
        delete gfx;
        gfx = nullptr;
    }

    if (lvl)
    {
        lvl->deinit();
        delete lvl;
        lvl = nullptr;
    }

    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();

    l->info("Engine deinit end");
    l = nullptr;
}

result engine::run()
{
    l->info("Engine run!");
    bool should_run = true;
    bool should_render = true;
    SDL_Event event{};
    while (should_run)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                should_run = false;
                break;
            }
            if (event.type == SDL_EVENT_WINDOW_MINIMIZED)
            {
                should_render = false;
                break;
            }
            if (event.type == SDL_EVENT_WINDOW_RESTORED)
            {
                should_render = true;
            }
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (const ImGuiIO& io = ImGui::GetIO(); !io.WantCaptureMouse && !io.WantCaptureKeyboard)
            {
                if (const auto res = lvl->process_sdl_event(event); res != result::ok)
                {
                    return res;
                }
            };
        }
        if (!should_render)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (!should_run) break;
        SDL_SetWindowRelativeMouseMode(window, !lvl->rls.cursor_enabled);
        if (!lvl->rls.cursor_enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        if (const auto res = this->run_frame(); res != result::ok)
        {
            l->error(std::format("frame failed: {}", static_cast<uint8_t>(res)));
            return res;
        }
    }

    return result::ok;
}

result engine::run_frame()
{
    uint64_t current_frame_time;
    std::chrono::time_point<std::chrono::system_clock> render_start;

    {
        // Start timing
        SDL_Time tick = 0;
        if (!SDL_GetCurrentTime(&tick))
        {
            l->error(std::format("Error getting current SDL tick: {}", SDL_GetError()));
            return result::error;
        }
        current_frame_time = tick - start_time;
        render_start = std::chrono::system_clock::now();
    }
    {
        // Prepare frame
        lvl->setup_frame();
    }
    // Track update time
    const auto update_start = std::chrono::system_clock::now();
    {
        // Update
        const uint64_t delta_time{current_frame_time - last_frame_time};
        double dt = static_cast<double>(delta_time) / sdl_time_to_seconds;
        if (dt > 0.25) dt = 0.25;

        if (const auto res = lvl->update(gfx->viewport_width, gfx->viewport_height, dt); res != result::ok)
        {
            return res;
        }
    }
    {
        // Process updates
        if (const auto res = lvl->process(); res != result::ok)
        {
            return res;
        }
        if (const auto res = gfx->update(lvl->rls, &lvl->wls); res != result::ok)
        {
            return res;
        }
    }
    {
        // Record update time
        const auto end = std::chrono::system_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - update_start);
        stats.level_update_time = elapsed.count() / 1000.f;
    }
    {
        // Render
        if (const auto res = gfx->render(stats); res != result::ok)
        {
            return res;
        }
    }
    {
        // Track timing
        const auto end = std::chrono::system_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - render_start);
        stats.frame_time = elapsed.count() / 1000.f;
        stats.r_fps = 1.f / (stats.frame_time / 1000.f);
        stats.a_fps = std::numbers::pi_v<float> * stats.r_fps;
        stats.d_fps = (std::numbers::pi_v<float> * stats.r_fps) * (180.f / std::numbers::pi_v<float>);
        last_frame_time = current_frame_time;
    }

    FrameMark;
    return result::ok;
}
