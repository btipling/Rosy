#include "Engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <print>

namespace rosy
{
	///// Log

	void log::debug(const std::string_view log_message) const
	{
		if (level != log_level::debug) return;
		std::cout << log_message; // Intentionally do not use line endings automatically for debugging.
	}

	void log::info(const std::string_view log_message) const
	{
		if (level > log_level::info) return;
		std::cout << log_message << std::endl;
	}

	void log::warn(const std::string_view log_message) const
	{
		if (level > log_level::warn) return;
		std::cout << log_message << std::endl;
	}

	void log::error(const std::string_view log_message) const
	{
		if (level == log_level::disabled) return;
		std::cerr << log_message << std::endl;
	}

	//// Engine
	
	result engine::init()
	{
		l = new log{ log_level::debug };
		l->info("Engine initializing.");
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			l->error(std::format("SDL initialization failed: {}\n", SDL_GetError()));
			return result::error;
		}

		int width = 640;
		int height = 480;
		int displays_count = 0;
		const auto display_ids = SDL_GetDisplays(&displays_count);
		if (!display_ids || displays_count <= 0)
		{
			l->error(std::format("Failed to get SDL display info: {}\n", SDL_GetError()));
			return result::error;
		}

		SDL_Rect display_bounds = {};
		if (SDL_GetDisplayBounds(*display_ids, &display_bounds)) {
			l->debug("Got display bounds\n");
			width = static_cast<int>(std::floor(static_cast<float>(display_bounds.w) * 0.75));
			height = static_cast<int>(std::floor(static_cast<float>(display_bounds.h) * 0.75));
		} else
		{
			l->error(std::format("SDL getting display bounds failed: {}\n", SDL_GetError()));
			return result::error;
		}

		constexpr SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
		window = SDL_CreateWindow("Rosy", width, height, window_flags);
		if (!window)
		{
			l->error(std::format("Window creation failed: {}\n", SDL_GetError()));
			return result::error;
		}
		l->info("Engine initialized.");
		return result::ok;
	}

	void engine::deinit()
	{
		l->info("Engine deinit!");
		if (window) {
			SDL_DestroyWindow(window);
			window = nullptr;
		}
		if (l)
		{
			delete l;
			l = nullptr;
		}
		SDL_Quit();
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

}
