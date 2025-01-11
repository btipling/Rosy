#include "Engine.h"

int main(int argc, char* argv[])
{
	std::cout << "Rosy Engine Starting" << std::endl;
	rosy::engine engine{};
	if (const rosy::result res = engine.init(); res != rosy::result::ok) {
		engine.deinit();
		return 1;
	}
	if (const rosy::result res = engine.run(); res != rosy::result::ok) {
		engine.deinit();
		return 1;
	}
	engine.deinit();
	return 0;
}

namespace rosy
{
	//// Engine
	result engine::init()
	{
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
			return result::error;
		}
		std::cout << "Engine init!" << std::endl;

		int width = 640;
		int height = 480;
		int displays_count = 0;
		const auto display_ids = SDL_GetDisplays(&displays_count);
		if (!display_ids || displays_count <= 0)
		{
			std::cerr << "Failed to get SDL display info: " << SDL_GetError() << std::endl;
			return result::error;
		}

		SDL_Rect display_bounds = {};
		if (SDL_GetDisplayBounds(*display_ids, &display_bounds)) {
			std::cout << "Got display bounds" << std::endl;
			width = static_cast<int>(std::floor(static_cast<float>(display_bounds.w) * 0.75));
			height = static_cast<int>(std::floor(static_cast<float>(display_bounds.h) * 0.75));
		} else
		{
			std::cerr << "SDL getting display bounds failed: " << SDL_GetError() << std::endl;
			return result::error;
		}

		constexpr SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
		window = SDL_CreateWindow("Rosy", width, height, window_flags);
		if (!window)
		{
			std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
			return result::error;
		}
		return result::ok;
	}

	void engine::deinit()
	{
		std::cout << "Engine deinit!" << std::endl;
		if (window) {
			SDL_DestroyWindow(window);
			window = nullptr;
		}
		SDL_Quit();
	}

	result engine::run()
	{
		std::cout << "Engine run!" << std::endl;
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