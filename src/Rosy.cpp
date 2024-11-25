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

bool eventHandler(void* userdata, SDL_Event* event) {
	 Rhi* rhi = (Rhi*)userdata;
	 VkResult result;
	switch (event->type) {
	case SDL_EVENT_WINDOW_RESIZED:
		SDL_Window* window = SDL_GetWindowFromID(event->window.windowID);
		result = rhi->resize_swapchain(window);
		if (result != VK_SUCCESS) {
			rosy_utils::DebugPrintA("resizing-event: rhi failed to resize swapchain %d\n", result);
			return false;
		}
		result = rhi->draw_frame();
		if (result != VK_SUCCESS) {
			rosy_utils::DebugPrintA("resizing-event: rhi draw failed %d\n", result);
			return false;
		}
		break;
	}
	return true;
}

int main(int argc, char* argv[])
{

	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;

	SDL_Init(SDL_INIT_VIDEO);

	int width = 640;
	int height = 480;
	int maxWidth = 640;
	int maxHeight = 480;
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
		maxWidth = displayBounds.w;
		maxHeight = displayBounds.h;
		width = displayBounds.w * 0.75;
		height = displayBounds.h * 0.75;
	}

	SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	window = SDL_CreateWindow("Rosy", width, height, windowFlags);

	rosy_config::Config cfg = {
		.maxWindowWidth = maxWidth,
		.maxWindowHeight = maxHeight,
	};
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


	SDL_AddEventWatch(eventHandler, rhi);

	bool shouldRun = true;
	bool shouldRender = true;
	bool resizeRequested = false;
	while (shouldRun) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				shouldRun = false;
				break;
			}
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
				shouldRender = false;
			}
			if (event.type == SDL_EVENT_WINDOW_RESTORED) {
				shouldRender = true;
			}
			if (event.type == SDL_EVENT_WINDOW_RESIZED) {
				rosy_utils::DebugPrintA("SDL_EVENT_WINDOW_RESIZED\n");
				resizeRequested = true;
			}
			if (!shouldRender) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			if (resizeRequested) {
				rosy_utils::DebugPrintA("resizing swapchain\n");
				result = rhi->resize_swapchain(window);
				if (result != VK_SUCCESS) {
					rosy_utils::DebugPrintA("rhi failed to resize swapchain %d\n", result);
					shouldRun = false;
					break;
				}
				resizeRequested = false;
			}

			{
				ImGui_ImplVulkan_NewFrame();
				ImGui_ImplSDL3_NewFrame();
				ImGui::NewFrame();
				result = rhi->draw_ui();
				if (result != VK_SUCCESS) {
					if (result == VK_ERROR_OUT_OF_DATE_KHR) {
						resizeRequested = true;
						break;
					}
					rosy_utils::DebugPrintA("rhi draw ui failed %d\n", result);
					shouldRun = false;
					break;
				}
				ImGui::Render();
			}

			result = rhi->draw_frame();
			if (result != VK_SUCCESS) {
				if (result == VK_ERROR_OUT_OF_DATE_KHR) {
					rosy_utils::DebugPrintA("swapchain out of date\n");
					resizeRequested = true;
					break;
				}
				rosy_utils::DebugPrintA("rhi draw failed %d\n", result);
				shouldRun = false;
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
