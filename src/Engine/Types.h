#pragma once
#include <cstdint>

namespace rosy
{
	enum class result : uint8_t
	{
		ok,
		error,
		invalid_argument,
		allocation_failure,
		graphics_init_failure,
		graphics_frame_failure,
		graphics_swapchain_failure,
		overflow,
	};

	enum class log_level : uint8_t
	{
		debug,
		info,
		warn,
		error,
		disabled,
	};

	struct config {
		int max_window_width = 0;
		int max_window_height = 0;
		uint32_t device_vendor = 4318;
		bool enable_validation_layers = true;
	};

}
