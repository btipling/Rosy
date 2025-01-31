#pragma once
#include "Types.h"
#include "Telemetry.h"
#include "Camera.h"
#include "../Packager/Asset.h"

namespace rosy {

	struct level
	{
		read_level_state rls{};
		write_level_state wls{};
		std::vector<graphics_object> graphics_objects;

		result init(log* new_log, [[maybe_unused]] config new_cfg, camera* new_cam);
		result set_asset(const rosy_packager::asset& new_asset);
		result update();
		void deinit();
	};
}
