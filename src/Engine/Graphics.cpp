#include "Graphics.h"

using namespace rosy;

//// Graphics

result graphics::init(SDL_Window* new_window, log const* new_log)
{
	window = new_window;
	l = new_log;
	l->info("Graphics init start");

	// Initialize graphics resources code goes here

	l->info("Graphics init done");
	return result::ok;
}

void graphics::deinit()
{
	l->info("Graphics deinit start");

	// De-initialize graphics resources code goes here

	l->info("Graphics deinit end");
	l = nullptr;
}

