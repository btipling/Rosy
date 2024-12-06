// Rosy.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Rosy.h"
#include <SDL3/SDL_main.h>
#include <thread>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define VOLK_IMPLEMENTATION
#include "volk/volk.h"
#define IMGUI_IMPL_VULKAN_USE_VOLK
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "rhi/rhi.h"
#include "config/Config.h"
#include "utils/utils.h"
#include "scene/scene_one/scene_one.h"

struct render_loop
{
	bool should_run = true;
	bool should_render = true;
	bool resize_requested = false;
	bool scene_loaded = false;
	bool show_cursor = true;
};

void render(const SDL_Event* event, rhi* renderer, render_loop* loop_ctx, scene_one* scene);  // NOLINT(misc-use-internal-linkage)

struct handler_data {
	rhi* renderer;
	scene_one* scene;
	render_loop* loop_ctx;
};


// ReSharper disable once CppParameterMayBeConstPtrOrRef
static bool event_handler(void* userdata, SDL_Event* event) {  // NOLINT(misc-use-anonymous-namespace)
	const auto data = static_cast<handler_data*>(userdata);
	VkResult result;
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (event->type) {
	case SDL_EVENT_WINDOW_RESIZED:
		SDL_Window* window = SDL_GetWindowFromID(event->window.windowID);
		result = data->renderer->resize_swapchain(window);
		if (result != VK_SUCCESS) {
			rosy_utils::debug_print_a("resizing-event: rhi failed to resize swapchain %d\n", result);
			return false;
		}
		bool resize_requested;
		bool should_run;
		bool scene_loaded;
		render(event, data->renderer, data->loop_ctx, data->scene);
		break;
	}
	return true;
}
int main(int argc, char* argv[])
{
	SDL_Window* window = nullptr;
	SDL_Renderer* sdl_renderer = nullptr;

	SDL_Init(SDL_INIT_VIDEO);

	int width = 640;
	int height = 480;
	int max_width = 640;
	int max_height = 480;
	int displays_count = 0;

	const auto display_ids = SDL_GetDisplays(&displays_count);
	if (displays_count == 0) {
		const auto err = SDL_GetError();
		rosy_utils::debug_print_a("SDL error: %s\n", err);
		abort();
	}

	// TODO: don't always get the first display
	SDL_Rect display_bounds = {};
	if (SDL_GetDisplayBounds(*display_ids, &display_bounds)) {
		max_width = display_bounds.w;
		max_height = display_bounds.h;
		width = static_cast<int>(std::floor(static_cast<float>(display_bounds.w) * 0.75));
		height = static_cast<int>(std::floor(static_cast<float>(display_bounds.h) * 0.75));
	}

	constexpr SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	window = SDL_CreateWindow("Rosy", width, height, window_flags);

	const rosy_config::Config cfg = {
		.maxWindowWidth = max_width,
		.maxWindowHeight = max_height,
	};
	rosy_config::debug();

	const std::unique_ptr<rhi> renderer(new rhi{ cfg });

	VkResult result = renderer->init(window);
	if (result != VK_SUCCESS) {
		rosy_utils::debug_print_a("rhi init failed %d\n", result);
		{
			SDL_DestroyRenderer(sdl_renderer);
			SDL_DestroyWindow(window);
			SDL_Quit();
		}
		return 1;
	}
	renderer->debug();

	scene_one scene = {};
	render_loop loop_ctx{};

	handler_data h_data = {
		.renderer = renderer.get(),
		.scene = &scene,
		.loop_ctx = &loop_ctx,
	};

	SDL_AddEventWatch(event_handler, static_cast<void*>(&h_data));
	SDL_Event event{};
	SDL_HideCursor();
	while (loop_ctx.should_run) {
		while (SDL_PollEvent(&event)) {

			if (event.type == SDL_EVENT_KEY_UP) {
				if (event.key.key == SDLK_C)
				{
					if (loop_ctx.show_cursor)
					{
						loop_ctx.show_cursor = false;
						SDL_HideCursor();

					}
					else
					{
						loop_ctx.show_cursor = true;
						SDL_ShowCursor();
					}
				}
			}
			ImGuiIO& io = ImGui::GetIO();
			if (!loop_ctx.show_cursor)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_None);
			}
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				loop_ctx.should_run = false;
				break;
			}
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
				loop_ctx.should_render = false;
			}
			if (event.type == SDL_EVENT_WINDOW_RESTORED) {
				loop_ctx.should_render = true;
			}
			if (event.type == SDL_EVENT_WINDOW_RESIZED) {
				rosy_utils::debug_print_a("SDL_EVENT_WINDOW_RESIZED\n");
				loop_ctx.resize_requested = true;
			}
			if (!loop_ctx.should_render) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			if (loop_ctx.resize_requested) {
				rosy_utils::debug_print_a("resizing swapchain\n");
				result = renderer->resize_swapchain(window);
				if (result != VK_SUCCESS) {
					rosy_utils::debug_print_a("rhi failed to resize swapchain %d\n", result);
					loop_ctx.should_run = false;
					break;
				}
				loop_ctx.resize_requested = false;
			}

			render(&event, renderer.get(), &loop_ctx, &scene);
		}
	}
	{
		rh::ctx ctx;
		if (std::expected<rh::ctx, VkResult> opt_ctx = renderer->current_frame_data(&event); opt_ctx.has_value())
		{
			ctx = opt_ctx.value();
		}
		else
		{
			rosy_utils::debug_print_a("no available frame data\n");
		}
		if (scene.deinit(ctx) == rh::result::error)
		{
			rosy_utils::debug_print_a("scene deinit failed\n");
		}
	}
	renderer->deinit();
	{
		SDL_DestroyRenderer(sdl_renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}
	return 0;
}

void render(const SDL_Event* event, rhi* renderer, render_loop* loop_ctx, scene_one* scene)
{
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		VkResult result = renderer->draw_ui();
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				loop_ctx->resize_requested = true;
				return;
			}
			rosy_utils::debug_print_a("rhi draw ui failed %d\n", result);
			loop_ctx->should_run = false;
			return;
		}
		{
			rh::ctx ctx;
			if (std::expected<rh::ctx, VkResult> opt_ctx = renderer->current_frame_data(event); opt_ctx.has_value())
			{
				ctx = opt_ctx.value();
			}
			else
			{
				rosy_utils::debug_print_a("no available frame data\n");
				loop_ctx->should_run = false;
				return;
			}
			if (const auto scene_result = scene->draw_ui(ctx); scene_result != rh::result::ok)
			{
				rosy_utils::debug_print_a("scene ui draw failed %d\n", result);
				loop_ctx->should_run = false;
			}
		}
		ImGui::Render();


		result = renderer->begin_frame();
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				rosy_utils::debug_print_a("swapchain out of date\n");
				loop_ctx->resize_requested = true;
				return;
			}
			rosy_utils::debug_print_a("rhi draw failed %d\n", result);
			loop_ctx->should_run = false;
			return;
		}
		{
			rh::ctx ctx;
			if (std::expected<rh::ctx, VkResult> opt_ctx = renderer->current_frame_data(event); opt_ctx.has_value())
			{
				ctx = opt_ctx.value();
			}
			else
			{
				rosy_utils::debug_print_a("no available frame data\n");
				loop_ctx->should_run = false;
				return;
			}
			if (!loop_ctx->scene_loaded)
			{
				if (const auto scene_result = scene->build(ctx); scene_result != rh::result::ok)
				{
					rosy_utils::debug_print_a("scene build failed %d\n", result);
					loop_ctx->should_run = false;
					return;
				}
				loop_ctx->scene_loaded = true;
			}
			ctx.mouse_enabled = !loop_ctx->show_cursor;
			if (const auto scene_result = scene->draw(ctx); scene_result != rh::result::ok)
			{
				rosy_utils::debug_print_a("scene draw failed %d\n", result);
				loop_ctx->should_run = false;
				return;
			}
		}

		result = renderer->end_frame();
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				rosy_utils::debug_print_a("swapchain out of date\n");
				loop_ctx->resize_requested = true;
				return;
			}
			rosy_utils::debug_print_a("rhi draw failed %d\n", result);
			loop_ctx->should_run = false;
			return;
		}
	}
}