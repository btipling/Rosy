#pragma once
#include <cmath>
#include <type_traits>

namespace rosy_math {
	template<typename T>
	bool is_equal(T a, T b, T epsilon = T(0.0001)) requires std::is_floating_point_v<T>
	{
		return std::abs(a - b) <= epsilon;
	}
}