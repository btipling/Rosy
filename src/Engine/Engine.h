#pragma once
#include "Types.h"
#include "Telemetry.h"
#include "Graphics.h"
#include "Camera.h"
#include "Level.h"


// ReSharper disable once CppInconsistentNaming
typedef struct SDL_Window SDL_Window;

namespace rosy
{
	struct engine
	{
		log* l{ nullptr };
		SDL_Window* window{ nullptr };
		uint64_t start_time{ 0 };
		uint64_t last_frame_time{ 0 };
		level* lvl{ nullptr };
		graphics* gfx{ nullptr };
		bool render_ui{ true };
		bool cursor_enabled{ true };
		engine_stats stats{};

		[[nodiscard]] result init();
		[[nodiscard]] result run();
		[[nodiscard]] result render();
		void deinit();
	};
}
