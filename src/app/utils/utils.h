#pragma once

#include <SDL3/SDL.h>

namespace rosy_utils
{
	struct state_debouncer
	{
		SDL_Time last_toggled = 0;
		SDL_Time delay = 0;
		bool state = true;
		void toggle();
	};

	void debug_print_w(const wchar_t* format, ...);
	void debug_print_a(const char* format, ...);
}
