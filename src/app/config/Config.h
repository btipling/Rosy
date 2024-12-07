#pragma once
#include <cstdint>


namespace rosy_config {

	struct config {
		int max_window_width = 0;
		int max_window_height = 0;
		uint32_t device_vendor = 4318;
		bool enable_validation_layers = true;
	};

	void debug();
}