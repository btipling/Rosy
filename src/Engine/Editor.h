#pragma once
#include "Types.h"
#include "Telemetry.h"


namespace rosy {

	struct editor
	{
		[[nodiscard]] result init(log* new_log, [[maybe_unused]] config new_cfg);
		void deinit();
	};
}