#pragma once
#include "../Rosy.h"
#include "rhi/rhi.h"
#include "config/Config.h"
#include "scene/scene.h"
#include "scene/scene_one/scene_one.h"
#include "utils/utils.h"

constexpr int max_width = 640;
constexpr int max_height = 480;

class app
{
public:
	rosy_config::config cfg{
			.max_window_width = max_width,
			.max_window_height = max_height,
	};
	rhi renderer = rhi{ &cfg };
	int init();
	int run();
	int deinit();
	void render(const SDL_Event* event);
private:
	bool should_run_ = true;
	bool should_render_ = true;
	bool resize_requested_ = false;
	bool scene_loaded_ = false;
	rosy_utils::state_debouncer show_cursor_ = { .state = true };
	SDL_Window* window_ = nullptr;
	std::unique_ptr <scene> scene_{};
	scene_selector scene_selector_{};

	void end_rendering(const char* message);
	void render_ui(const SDL_Event* event);
	void render_scene(const SDL_Event* event);

};

