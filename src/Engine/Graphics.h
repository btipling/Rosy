#pragma once
#include "Types.h"
#include "Telemetry.h"

// ReSharper disable once CppInconsistentNaming
typedef struct SDL_Window SDL_Window;

namespace rosy
{
	struct graphics
	{
		SDL_Window* window{ nullptr };
		log const* l{ nullptr };

		result init(SDL_Window* new_window, log const* new_log);
		void deinit();
	};
}