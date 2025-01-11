#include "Engine.h"
#include <format>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

using namespace rosy;

//// Engine

result engine::init()
{
	l = new(std::nothrow) log{ log_level::debug };
	if (l == nullptr)
	{
		return result::error;
	}

	l->info("Engine init begin");

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
			l->debug("Got display bounds\n");
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
	}

	// Graphics engine initialization
	{
		gfx = new(std::nothrow) graphics{};
		if (gfx == nullptr)
		{
			l->error("Error allocating graphics engine");
			return result::error;
		}
		if (auto const res = gfx->init(window, l); res != result::ok)
		{
			l->error(std::format("Graphics creation failed: {}", static_cast<uint8_t>(res)));
			return result::error;
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

result engine::run()
{
	l->info("Engine run!");
	bool should_run = true;
	SDL_Event event{};
	while (should_run)
	{
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				should_run = false;
				break;
			}
		}
		if (!should_run) break;
	}
	return result::ok;
}
