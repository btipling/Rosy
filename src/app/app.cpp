#include "app.h"
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
#include "config/Config.h"
#include "scene/scene_one/scene_one.h"

// ReSharper disable once CppParameterMayBeConstPtrOrRef
static bool event_handler(void* userdata, SDL_Event* event) {  // NOLINT(misc-use-anonymous-namespace)
	const auto app_i = static_cast<app*>(userdata);
	VkResult result;
	// ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
	switch (event->type) {
	case SDL_EVENT_WINDOW_RESIZED:
		SDL_Window* window = SDL_GetWindowFromID(event->window.windowID);
		result = app_i->renderer.resize_swapchain(window);
		if (result != VK_SUCCESS) {
			rosy_utils::debug_print_a("resizing-event: rhi failed to resize swapchain %d\n", result);
			return false;
		}
		app_i->render(event);
		break;
	}
	return true;
}


int app::init()
{
	{
		SDL_Init(SDL_INIT_VIDEO);

		int width = 640;
		int height = 480;
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
			cfg.max_window_width = display_bounds.w;
			cfg.max_window_height = display_bounds.h;
			width = static_cast<int>(std::floor(static_cast<float>(display_bounds.w) * 0.75));
			height = static_cast<int>(std::floor(static_cast<float>(display_bounds.h) * 0.75));
		}

		constexpr SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
		window_ = SDL_CreateWindow("Rosy", width, height, window_flags);
	}
	if (const VkResult result = renderer.init(window_); result != VK_SUCCESS) {
		rosy_utils::debug_print_a("rhi init failed %d\n", result);
		return 1;
	}
	renderer.debug();
	SDL_AddEventWatch(event_handler, static_cast<void*>(this));
	return 0;
}

int app::deinit()
{
	{
		// Deinit scene_
		rh::ctx ctx;
		if (const std::expected<rh::ctx, VkResult> opt_ctx = renderer.current_frame_data(nullptr); opt_ctx.has_value())
			ctx = opt_ctx.value();
		else rosy_utils::debug_print_a("no available frame data\n");
		if (scene_.deinit(ctx) == rh::result::error) rosy_utils::debug_print_a("scene_ deinit failed\n");
	}
	{
		// Deinit renderer
		renderer.deinit();
	}
	{
		// Deinit SDL
		SDL_DestroyWindow(window_);
		SDL_Quit();
	}
	return 0;
}

int app::run()
{
	SDL_Event event{};
	while (should_run_) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_KEY_UP) {
				if (event.key.key == SDLK_C) {
					show_cursor_.toggle();
					if (show_cursor_.state) SDL_SetWindowMouseGrab(window_, false);
					else SDL_SetWindowMouseGrab(window_, true);
				}
			}
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				return 0;
			}
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
				should_render_ = false;
			}
			if (event.type == SDL_EVENT_WINDOW_RESTORED) {
				should_render_ = true;
			}
			if (event.type == SDL_EVENT_WINDOW_RESIZED) {
				resize_requested_ = true;
			}
			if (!should_render_) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}
			if (resize_requested_) {
				if (const VkResult result = renderer.resize_swapchain(window_); result != VK_SUCCESS) {
					rosy_utils::debug_print_a("rhi failed to resize swapchain %d\n", result);
					return 2;
				}
				resize_requested_ = false;
			}
			render(&event);
		}
		render(nullptr);
	}
	return 0;
}

void app::render(const SDL_Event* event)
{
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		VkResult result = renderer.draw_ui();
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				resize_requested_ = true;
				return;
			}
			rosy_utils::debug_print_a("rhi draw ui failed %d\n", result);
			should_run_ = false;
			return;
		}
		{
			rh::ctx ctx;
			if (std::expected<rh::ctx, VkResult> opt_ctx = renderer.current_frame_data(event); opt_ctx.has_value())
			{
				ctx = opt_ctx.value();
			}
			else
			{
				rosy_utils::debug_print_a("no available frame data\n");
				should_run_ = false;
				return;
			}
			if (const auto scene_result = scene_.draw_ui(ctx); scene_result != rh::result::ok)
			{
				rosy_utils::debug_print_a("scene_ ui draw failed %d\n", result);
				should_run_ = false;
			}
		}
		ImGuiIO& io = ImGui::GetIO();
		if (!show_cursor_.state)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		}
		ImGui::Render();


		result = renderer.begin_frame();
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				rosy_utils::debug_print_a("swapchain out of date\n");
				resize_requested_ = true;
				return;
			}
			rosy_utils::debug_print_a("rhi draw failed %d\n", result);
			should_run_ = false;
			return;
		}
		{
			rh::ctx ctx;
			if (const std::expected<rh::ctx, VkResult> opt_ctx = renderer.current_frame_data(event); opt_ctx.has_value())
			{
				ctx = opt_ctx.value();
			}
			else
			{
				rosy_utils::debug_print_a("no available frame data\n");
				should_run_ = false;
				return;
			}
			if (!scene_loaded_)
			{
				if (const auto scene_result = scene_.build(ctx); scene_result != rh::result::ok)
				{
					rosy_utils::debug_print_a("scene_ build failed %d\n", result);
					should_run_ = false;
					return;
				}
				scene_loaded_ = true;
			}
			ctx.mouse_enabled = !show_cursor_.state;
			if (const auto scene_result = scene_.update(ctx); scene_result != rh::result::ok)
			{
				rosy_utils::debug_print_a("scene_ update failed %d\n", result);
				should_run_ = false;
				return;
			}
			if (const auto scene_result = scene_.draw(ctx); scene_result != rh::result::ok)
			{
				rosy_utils::debug_print_a("scene_ draw failed %d\n", result);
				should_run_ = false;
				return;
			}
		}

		result = renderer.end_frame();
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				rosy_utils::debug_print_a("swapchain out of date\n");
				resize_requested_ = true;
				return;
			}
			rosy_utils::debug_print_a("rhi draw failed %d\n", result);
			should_run_ = false;
		}
	}
}
