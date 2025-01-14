#pragma once
#include "Types.h"
#include "Telemetry.h"

// ReSharper disable once CppInconsistentNaming
typedef struct SDL_Window SDL_Window;


namespace rosy
{

	struct graphics
	{
		log const* l{ nullptr };

		result init(SDL_Window* new_window, log const* new_log, config cfg);
		result render();
		result resize();
		void deinit();
	};
}
