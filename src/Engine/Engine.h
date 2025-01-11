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
		SDL_Window* window = nullptr;

		result init();
		result run();
		void deinit();
	};
}