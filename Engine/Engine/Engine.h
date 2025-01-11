#pragma once
#pragma warning(4)
#pragma warning(error)
#include <iostream>

namespace rosy
{

	enum class result: uint8_t
	{
		ok,
		error,
	};

	struct engine
	{
		result init();
		result run();
		void deinit();
	};
}