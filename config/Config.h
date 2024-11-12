#pragma once
#include <cstdint>


namespace rosy_config {

	struct Config {
		uint32_t device_vendor = 4318;
	};

	void debug();
}