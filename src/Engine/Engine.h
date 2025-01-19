#pragma once
#include "Types.h"
#include "Telemetry.h"
#include "Graphics.h"
#include "Camera.h"


// ReSharper disable once CppInconsistentNaming
typedef struct SDL_Window SDL_Window;

namespace rosy
{
	struct engine
	{
		log* l{ nullptr };
		SDL_Window* window{ nullptr };
		camera* cam{ nullptr };
		graphics* gfx{ nullptr };

		[[nodiscard]] result init();
		[[nodiscard]] result run() const;
		void deinit();
	};
}
