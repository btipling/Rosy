#pragma once
#include "pch.h"

#include <cstdint>

namespace rhi_lib {
	enum class result : uint8_t
	{
		ok,
		error,
		limit_reached,
	};
}
