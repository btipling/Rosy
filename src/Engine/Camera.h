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

		float starting_x{ 0 };
		float starting_y{ 0 };
		float starting_z{ 0 };

		double g{ 0.5f }; // projection plane distance
		double s{ 1.f }; // aspect ratio
		double n{ 0.1f }; // near plane
		double f{ 1000.f }; // far plane
		double fov{ 70.0f }; // field of view
		std::array<float, 16> p; // projection
		std::array<float, 16> v; // view
		std::array<float, 16> vp; // view projection
		std::array<float, 16> r; // camera rotation
		std::array<float, 4> position; // camera projection

		result init(log const* new_log, config cfg);
		void deinit() const;
		result update(uint32_t viewport_width, uint32_t viewport_height);
		result process_sdl_event(const SDL_Event& event, bool mouse_enabled);
	};
}
