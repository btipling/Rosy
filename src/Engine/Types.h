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
	};

	enum class log_level : uint8_t
	{
		debug,
		info,
		warn,
		error,
		disabled,
	};
}
