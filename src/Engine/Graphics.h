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
		[[nodiscard]] result set_asset(const rosy_packager::asset& a, std::vector<graphics_object> graphics_objects) const;
		[[nodiscard]] result update(const std::array<float, 16>& v, const std::array<float, 16>& p, const std::array<float, 16>& vp, std::array<float, 4> cam_pos);
		[[nodiscard]] result render(bool render_ui, const engine_stats& stats);
		[[nodiscard]] result resize();
		void deinit();
	};
}
