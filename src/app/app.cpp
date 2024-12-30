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
#include "scene/scene_two/scene_two.h"

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
	scene_selector_ = scene_selector{};
	scene_selector_.updated = true;
	scene_selector_.selected_scene = 1;
	return 0;
}

int app::deinit()
{
	{
		// Deinit scene_
		rh::ctx ctx;
		if (const std::expected<rh::ctx, VkResult> opt_ctx = renderer.current_render_ctx(nullptr); opt_ctx.has_value())
			ctx = opt_ctx.value();
		else rosy_utils::debug_print_a("no available frame data\n");
		if (scene_ != nullptr) {
			if (scene_->deinit(ctx) == rh::result::error) rosy_utils::debug_print_a("scene_ deinit failed\n");
		}
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

void app::change_scene()
{
	{
		// Deinit current scene_
		rh::ctx ctx;
		if (const std::expected<rh::ctx, VkResult> opt_ctx = renderer.current_render_ctx(nullptr); opt_ctx.has_value())
		{
			ctx = opt_ctx.value();
		}
		else end_rendering("no available frame data\n");
		if (scene_ != nullptr && scene_->deinit(ctx) == rh::result::error) end_rendering("scene_ deinit failed\n");
		scene_loaded_ = false;
	}
	{
		switch (scene_selector_.selected_scene)
		{
		case 0:
			scene_ = std::make_unique<scene_one>();
			break;
		case 1:
			scene_ = std::make_unique<scene_two>();
			break;
		default:
			return end_rendering("Invalid scene selected\n");
		}
	}
}

int app::run()
{
	SDL_Event event{};
	scene_selector_.updated = true;
	while (should_run_) {
		if (scene_selector_.updated)
		{
			rosy_utils::debug_print_a("Scene selection changed %d\n", scene_selector_.selected_scene);
			change_scene();
			scene_selector_.updated = false;
		}
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_KEY_UP) {
				if (event.key.key == SDLK_C) {
					show_cursor_.toggle();
					if (show_cursor_.state) SDL_SetWindowRelativeMouseMode(window_, false);
					else SDL_SetWindowRelativeMouseMode(window_, true);
				}
			}
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				return 0;
			}
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
				should_render_ = false;
				break;
			}
			if (event.type == SDL_EVENT_WINDOW_RESTORED) {
				should_render_ = true;
				resize_requested_ = true;
			}
			if (event.type == SDL_EVENT_WINDOW_RESIZED) {
				resize_requested_ = true;
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
		if (!should_render_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		render(nullptr);
	}
	return 0;
}

void app::end_rendering(const char* message)
{
	rosy_utils::debug_print_a(message);
	should_run_ = false;
	return;
}

void app::render(const SDL_Event* event)
{
	render_ui(event);
	render_scene(event);
}

void app::render_ui(const SDL_Event* event)
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	if (const VkResult result = renderer.draw_ui(); result != VK_SUCCESS) return end_rendering("rhi draw ui failed\n");
	{
		rh::ctx ctx;
		if (const std::expected<rh::ctx, VkResult> opt_ctx = renderer.current_render_ctx(event); opt_ctx.has_value()) ctx = opt_ctx.value();
		else return end_rendering("no available frame data\n");
		if (scene_ != nullptr)
		{
			if (const auto scene_result = scene_->draw_ui(ctx); scene_result != rh::result::ok)  return end_rendering("scene_ draw ui failed\n");
		}
	}
	scene_selector_.draw_ui();
	if (!show_cursor_.state) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
	ImGui::Render();
}

void app::render_scene(const SDL_Event* event)
{
	if (scene_ == nullptr) return;
	if (const VkResult result = renderer.begin_frame();  result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			rosy_utils::debug_print_a("swapchain out of date\n");
			resize_requested_ = true;
			return;
		}
		return end_rendering("rhi begin frame failed\n");
	}

	rh::ctx ctx;
	{
		// Init render context
		if (const std::expected<rh::ctx, VkResult> opt_ctx = renderer.current_shadow_pass_ctx(event); opt_ctx.has_value()) ctx = opt_ctx.value();
		else return end_rendering("no available frame data\n");
		ctx.mouse_enabled = !show_cursor_.state;
	}

	if (!scene_loaded_)
	{
		// Load scene
		if (const auto scene_result = scene_->build(ctx); scene_result != rh::result::ok) return end_rendering("scene_ build failed\n");
		scene_loaded_ = true;
		// Update scene
		if (const auto scene_result = scene_->update(ctx); scene_result != rh::result::ok) return end_rendering("scene_ update failed\n");
	}
	// Generate shadows

	if (const VkResult result = renderer.init_shadow_pass();  result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			rosy_utils::debug_print_a("swapchain out of date\n");
			resize_requested_ = true;
			return;
		}
		return end_rendering("rhi shadow pass failed\n");
	}

	constexpr int num_shadow_passes = 3;
	for (int i = 0; i < num_shadow_passes; i++) {
		if (const VkResult result = renderer.begin_shadow_pass(i);  result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				rosy_utils::debug_print_a("swapchain out of date\n");
				resize_requested_ = true;
				return;
			}
			return end_rendering("rhi shadow pass failed\n");
		}
		if (const auto scene_result = scene_->depth(ctx, i); scene_result != rh::result::ok) return end_rendering("scene_ depth failed %d\n");
		if (const VkResult result = renderer.end_shadow_pass();  result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				rosy_utils::debug_print_a("swapchain out of date\n");
				resize_requested_ = true;
				return;
			}
			return end_rendering("rhi shadow pass failed\n");
		}
	}

	{
		// Init render context
		if (const std::expected<rh::ctx, VkResult> opt_ctx = renderer.current_render_ctx(event); opt_ctx.has_value()) ctx = opt_ctx.value();
		else return end_rendering("no available frame data\n");
		ctx.mouse_enabled = !show_cursor_.state;
	}
	// Render Scene
	if (const VkResult result = renderer.render_pass();  result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			rosy_utils::debug_print_a("swapchain out of date\n");
			resize_requested_ = true;
			return;
		}
		return end_rendering("rhi render pass failed\n");
	}
	if (const auto scene_result = scene_->draw(ctx); scene_result != rh::result::ok) return end_rendering("scene_ draw failed\n");

	if (const VkResult result = renderer.end_frame(); result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			rosy_utils::debug_print_a("swapchain out of date\n");
			resize_requested_ = true;
			return;
		}
		return end_rendering("renderer end frame failed\n");
	}
}
