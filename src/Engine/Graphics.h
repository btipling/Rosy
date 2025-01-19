#pragma once
#include "Types.h"
#include "Telemetry.h"
#include "../Packager/Asset.h"

// ReSharper disable once CppInconsistentNaming
typedef struct SDL_Window SDL_Window;


namespace rosy
{

	struct graphics
	{
		log const* l{ nullptr };
		uint32_t viewport_width{ 0 };
		uint32_t viewport_height{ 0 };

		result init(SDL_Window* new_window, log const* new_log, config cfg);
		result set_asset(const rosy_packager::asset& a);
		result render();
		result resize();
		void deinit();
	};
}
