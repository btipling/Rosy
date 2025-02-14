#pragma once
#include "Types.h"
#include "Telemetry.h"
#include "../Packager/Asset.h"
#include <SDL3/SDL.h>

namespace rosy {

	struct level
	{
		read_level_state rls{};
		write_level_state wls{};
		graphics_object_update graphics_object_update_data{};
		std::vector<graphics_object> graphics_objects;
		size_t static_objects_offset{ 0 };
		size_t num_dynamic_objects{ 0 };

		result init(log* new_log, [[maybe_unused]] config new_cfg);
		result set_asset(const rosy_packager::asset& new_asset);
		result setup_frame();
		result update(const uint32_t viewport_width, const uint32_t viewport_height, double dt);
		result process();
		result process_sdl_event(const SDL_Event& event);
		void deinit();
	};
}


