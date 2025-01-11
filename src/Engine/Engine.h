#pragma once
#pragma warning(4)
#pragma warning(error)
#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

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