#include "Engine.h"

int main(int argc, char* argv[])
{
	std::cout << "Hello World!\n";
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
		SDL_Init(SDL_INIT_VIDEO);
		std::cout << "Engine init!\n";

		int width = 640;
		int height = 480;
		int displays_count = 0;
		const auto display_ids = SDL_GetDisplays(&displays_count);

		SDL_Rect display_bounds = {};
		if (SDL_GetDisplayBounds(*display_ids, &display_bounds)) {
			std::cout << "Got display bounds";
			width = static_cast<int>(std::floor(static_cast<float>(display_bounds.w) * 0.75));
			height = static_cast<int>(std::floor(static_cast<float>(display_bounds.h) * 0.75));
		}

		constexpr SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
		window = SDL_CreateWindow("Rosy", width, height, window_flags);
		return result::ok;
	}

	void engine::deinit()
	{
		std::cout << "Engine deinit!\n";
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	result engine::run()
	{
		std::cout << "Engine run!\n";
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