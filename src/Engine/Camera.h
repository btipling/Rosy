#pragma once
#include "Types.h"
#include "Telemetry.h"
#include <array>
#include <SDL3/SDL.h>

namespace rosy
{
	struct camera
	{
		log const* l{ nullptr };

		double g; // projection plane distance
		double s; // aspect ratio
		double n; // near plane
		double f; // far plane
		double fov; // field of view
		std::array<float, 16> p; // projection
		std::array<float, 16> v; // view
		std::array<float, 16> vp; // view projection
		std::array<float, 16> r; // camera rotation
		result init(log const* new_log, config cfg);
		void deinit();
		result process_sdl_event(const SDL_Event& event);
	};
}