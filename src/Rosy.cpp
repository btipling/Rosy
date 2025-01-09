// Rosy.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Rosy.h"
#include <SDL3/SDL_main.h>
#include "app/app.h"


#pragma warning(disable: 4100)
int main(int argc, char* argv[])
{
	app app_i{};
	if (const int res = app_i.init() != 0) return res;
	if (const int res = app_i.run() != 0) return res;
	return app_i.deinit();
}
#pragma warning(default: 4100)
