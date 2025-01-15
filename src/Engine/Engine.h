#pragma once
#include "Types.h"
#include "Telemetry.h"
#include "Graphics.h"


// ReSharper disable once CppInconsistentNaming
typedef struct SDL_Window SDL_Window;

namespace rosy
{
	struct engine
	{
		SDL_Window* window{ nullptr };
		log* l{ nullptr };
		graphics* gfx{ nullptr };

		result init();
		result run() const;
		void deinit();
	};
}
