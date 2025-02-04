#include "Engine.h"
#include "../Packager/Asset.h"

#include <chrono>
#include <format>
#include <thread>
#include <numbers>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include <tracy/Tracy.hpp>
#ifdef TRACY_ENABLED
#include <client/TracyProfiler.hpp>
#endif


using namespace rosy;

//// Engine

// ReSharper disable once CppParameterMayBeConstPtrOrRef
static bool event_handler(void* userdata, SDL_Event* event) {  // NOLINT(misc-use-anonymous-namespace)
	const auto eng = static_cast<engine*>(userdata);
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (event->type) {
	case SDL_EVENT_WINDOW_RESIZED:
		if (result res = eng->gfx->resize(); res != result::ok) {
			eng->l->error(std::format("resizing-event: gfx failed to resize swapchain {}\n", static_cast<uint8_t>(res)));
			return false;
		}
		if (result res = eng->render(); res != result::ok) {
			eng->l->error(std::format("resizing-event: gfx failed to render {}\n", static_cast<uint8_t>(res)));
			return false;
		}
		break;
	}
	return true;
}

result engine::init()
{
	l = new(std::nothrow) log{ log_level::debug };
	if (l == nullptr)
	{
		return result::allocation_failure;
	}
#ifdef ROSY_LOG_LEVEL_DEBUG
	l->level = rosy::log_level::debug;
#endif

	l->info("Engine init begin");
	rosy_packager::asset a{};
	{
		//a.asset_path = "..\\assets\\sponza\\sponza.rsy";
		a.asset_path = "..\\assets\\houdini\\exports\\Box_002\\Box_002.rsy";
		{
			if (const auto res = a.read(l); res != result::ok)
			{
				l->error("Failed to read the assets!");
				return result::error;
			}
		}
		a.shaders.push_back({ .path = "../shaders/out/basic.spv" });
		{
			if (const auto res = a.read_shaders(l); res != result::ok)
			{
				l->error("Failed to read shaders!");
				return result::error;
			}
		}
	}

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
		if (SDL_GetDisplayBounds(*display_ids, &display_bounds)) {
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

		if(!SDL_AddEventWatch(event_handler, static_cast<void*>(this)))
		{
			l->error(std::format("Failed to add event watcher: {}", SDL_GetError()));
			return result::error;
		}
	}

	// Level initialization
	{
		lvl = new(std::nothrow) level{};
		if (lvl == nullptr)
		{
			l->error("Error allocating level");
			return result::allocation_failure;
		}
		if (auto const res = lvl->init(l, cfg); res != result::ok)
		{
			l->error(std::format("Level creation failed: {}", static_cast<uint8_t>(res)));
			return res;
		}
		if (auto const res = lvl->set_asset(a); res != result::ok)
		{
			l->error(std::format("Asset setting on level failed: {}", static_cast<uint8_t>(res)));
			return res;
		}
	}

	// Graphics engine initialization
	{
		gfx = new(std::nothrow) graphics{};
		if (gfx == nullptr)
		{
			l->error("Error allocating graphics engine");
			return result::allocation_failure;
		}
		if (auto const res = gfx->init(window, l, cfg); res != result::ok)
		{
			l->error(std::format("Graphics creation failed: {}", static_cast<uint8_t>(res)));
			return res;
		}
		if (auto const res = gfx->set_asset(a, lvl->graphics_objects, &lvl->wls); res != result::ok)
		{
			l->error(std::format("Asset setting on graphics failed: {}", static_cast<uint8_t>(res)));
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

	if (window) {
		SDL_DestroyWindow(window);
		window = nullptr;
	}

	SDL_Quit();

	if (l)
	{
		l->info("Engine deinit end");
		delete l;
		l = nullptr;
	}
}

result engine::run()
{
	l->info("Engine run!");
	bool should_run = true;
	bool should_render = true;
	SDL_Event event{};
	while (should_run)
	{
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				should_run = false;
				break;
			}
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
				should_render = false;
				break;
			}
			if (event.type == SDL_EVENT_WINDOW_RESTORED) {
				should_render = true;
			}
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_KEY_UP) {
				if (event.key.key == SDLK_C)
				{
					cursor_enabled = !cursor_enabled;
					SDL_SetWindowRelativeMouseMode(window, !cursor_enabled);
				}
				if (event.key.key == SDLK_F1)
				{
					render_ui = !render_ui;
				}
			}
			if (const auto res = lvl->cam->process_sdl_event(event, !cursor_enabled); res != result::ok) {
				return res;
			}

		}
		if (!should_render) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		if (!should_run) break;
		if (!cursor_enabled) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		if (const auto res = render(); res != result::ok) {
			l->error(std::format("render failed: {}", static_cast<uint8_t>(res)));
			return res;
		}
	}

	return result::ok;
}

result engine::render()
{
	const auto render_start = std::chrono::system_clock::now();
	if (const auto res =  lvl->cam->update(gfx->viewport_width, gfx->viewport_height); res != result::ok) {
		return res;
	}
	{
		const auto update_start = std::chrono::system_clock::now();
		if (const auto res = lvl->update(); res != result::ok) {
			return res;
		}
		if (const auto res = gfx->update(lvl->rls, lvl->graphics_object_update_data); res != result::ok) {
			return res;
		}
		const auto end = std::chrono::system_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - update_start);
		stats.level_update_time = elapsed.count() / 1000.f;
	}
	if (const auto res = gfx->render(render_ui, stats); res != result::ok) {
		return res;
	}
	{
		const auto end = std::chrono::system_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - render_start);
		stats.frame_time = elapsed.count() / 1000.f;
		stats.r_fps = 1.f / (stats.frame_time / 1000.f);
		stats.a_fps = std::numbers::pi_v<float> *stats.r_fps;
		stats.d_fps = (std::numbers::pi_v<float> *stats.r_fps) * (180.f / std::numbers::pi_v<float>);
	}
	FrameMark;
	return result::ok;
}
