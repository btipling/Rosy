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

		[[nodiscard]] result init(SDL_Window* new_window, log const* new_log, config cfg);
		[[nodiscard]] result set_asset(const rosy_packager::asset& a, const std::vector<graphics_object>& graphics_objects, write_level_state* wls) const;
		[[nodiscard]] result update(const read_level_state& rls);
		[[nodiscard]] result render(bool render_ui, const engine_stats& stats);
		[[nodiscard]] result resize();
		void deinit();
	};
}
