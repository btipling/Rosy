#include "Engine.h"
#include "../Packager/Asset.h"

#include <chrono>
#include <format>
#include <thread>
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
			eng->l->error(std::format("resizing-event: rhi failed to resize swapchain {}\n", static_cast<uint8_t>(res)));
			return false;
		}
		eng->gfx->render();
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

	l->info("Engine init begin");
	rosy_packager::asset a{};
	a.asset_path = "../assets/demo_cube/demo_cube.rsy";
	{
		if (const auto res = a.read(); res != result::ok)
		{
			l->error("Failed to read the assets!");
			return result::error;
		}
	}
	a.shaders.push_back({ .path = "../shaders/out/basic.spv" });
	{
		if (const auto res = a.read_shaders(); res != result::ok)
		{
			l->error("Failed to read shaders!");
			return result::error;
		}
	}

	config cfg{};
	// SDL Window initialization.
	{
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			l->error(std::format("SDL initialization failed: {}", SDL_GetError()));
			return result::error;
		}

		int width = 640;
		int height = 480;
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
		if (auto const res = gfx->set_asset(a); res != result::ok)
		{
			l->error(std::format("Asset setting failed: {}", static_cast<uint8_t>(res)));
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

result engine::run() const
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
		}
		if (!should_render) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		if (!should_run) break;

		{
			if (const auto res = gfx->render(); res != result::ok) {
				return res;
			}
			FrameMark;
		}
	}
	return result::ok;
}
