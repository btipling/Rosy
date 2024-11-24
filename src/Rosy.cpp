// Rosy.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Rosy.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <thread>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define VOLK_IMPLEMENTATION
#include "volk/volk.h"
#define IMGUI_IMPL_VULKAN_USE_VOLK
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "rhi/RHI.h"
#include "config/Config.h"
#include "utils/utils.h"

int main(int argc, char* argv[])
{

	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;

	SDL_Init(SDL_INIT_VIDEO);

	int width = 640;
	int height = 480;
	int displaysCount = 0;
	auto displayIds = SDL_GetDisplays(&displaysCount);
	if (displaysCount == 0) {
		auto err = SDL_GetError();
		rosy_utils::DebugPrintA("SDL error: %s\n", err);
		SDL_free((void*)err);
		abort();
	}
	// TODO: don't always get the first display
	SDL_Rect displayBounds = {};
	if (SDL_GetDisplayBounds(*displayIds, &displayBounds)) {
		width = displayBounds.w;
		height = displayBounds.h;
	}

	window = SDL_CreateWindow("Rosy", width, height, SDL_WINDOW_VULKAN);

	rosy_config::Config cfg = {};
	rosy_config::debug();

	Rhi* rhi = new Rhi{ cfg };

	VkResult result = rhi->init(window);
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintA("rhi init failed %d\n", result);
		delete rhi;
		{
			SDL_DestroyRenderer(renderer);
			SDL_DestroyWindow(window);
			SDL_Quit();
		}
		return 1;
	}
	rhi->debug();

	bool should_run = true;
	bool should_render = true;
	while (should_run) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				should_run = false;
				break;
			}
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
				should_render = false;
			}
			if (event.type == SDL_EVENT_WINDOW_RESTORED) {
				should_render = true;
			}
			if (!should_render) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			{
				ImGui_ImplVulkan_NewFrame();
				ImGui_ImplSDL3_NewFrame();
				ImGui::NewFrame();
				result = rhi->drawUI();
				if (result != VK_SUCCESS) {
					rosy_utils::DebugPrintA("rhi draw ui failed %d\n", result);
					should_run = false;
					break;
				}
				ImGui::Render();
			}

			result = rhi->drawFrame();
			if (result != VK_SUCCESS) {
				rosy_utils::DebugPrintA("rhi draw failed %d\n", result);
				should_run = false;
				break;
			}
		}
	}
	delete rhi;
	{
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}
	return 0;
}
