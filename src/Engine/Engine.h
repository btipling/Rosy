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
		// Owned resources
		log* l{ nullptr };
		SDL_Window* window{ nullptr };
		level* lvl{ nullptr };
		graphics* gfx{ nullptr };

		// Rendering controls
		bool render_ui{ true };
		bool cursor_enabled{ true };

		// Timing
		uint64_t start_time{ 0 };
		uint64_t last_frame_time{ 0 };

		// Profiling
		engine_stats stats{};

		[[nodiscard]] result init();
		[[nodiscard]] result run();
		[[nodiscard]] result run_frame();
		void deinit();
	};
}
