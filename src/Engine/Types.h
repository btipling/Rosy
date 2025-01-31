﻿#pragma once
#include <array>
#include <vector>
#include <cstdint>

namespace rosy
{
	enum class result : uint8_t
	{
		ok,
		error,
		invalid_argument,
		allocation_failure,
		graphics_init_failure,
		graphics_frame_failure,
		graphics_swapchain_failure,
		open_failed,
		write_failed,
		read_failed,
		create_failed,
		update_failed,
		overflow,
	};

	enum class log_level : uint8_t
	{
		debug,
		info,
		warn,
		error,
		disabled,
	};

	struct config {
		int max_window_width = 0;
		int max_window_height = 0;
		uint32_t device_vendor = 4318;
		bool enable_validation_layers = true;
	};

	struct  surface_graphics_data
	{
		size_t mesh_index{ 0 };
		size_t graphics_object_index{ 0 };
		size_t material_index{ 0 };
		uint32_t index_count{ 0 };
		uint32_t start_index{ 0 };
		bool blended{ false };
	};

	struct graphics_object
	{
		std::vector<surface_graphics_data> surface_data{};
		std::array<float, 16> transform;
		std::array<float, 16> normal_transform;
	};

	struct engine_stats
	{
		float a_fps{ 0.f };
		float d_fps{ 0.f };
		float r_fps{ 0.f };
		float frame_time{ 0.f };
		float level_update_time{ 0.f };
	};

	enum class debug_object_type : uint8_t
	{
		line,
		circle,
		cross,
	};

	struct debug_object
	{
		debug_object_type type{ debug_object_type::line };
		std::array<float, 16> transform{};
		std::array<float, 4> color{};
	};

	struct read_camera
	{
		std::array<float, 16> v{};
		std::array<float, 16> p{};
		std::array<float, 16> vp{};
		std::array<float, 16> shadow_projection_near{};
		std::array<float, 4> position{};
	};

	struct read_level_state
	{
		read_camera cam{};
		bool debug_enabled{ false };
		bool reverse_winding_order_enabled{ false };
		bool cull_enabled{ true };
		bool wire_enabled{ true };
		std::array<float, 4> sunlight{};
		std::vector<debug_object> debug_objects{};
		bool depth_bias_enabled{ false };
		float depth_bias_constant{ 0.f };
		float depth_bias_clamp{ 0.f };
		float depth_bias_slope_factor{ 0.f };
	};

	struct write_level_state
	{
		float sun_distance{ 12.833f };
		float sun_pitch{ 5.141f };
		float sun_yaw{ 1.866f };
		float orthographic_depth{ 32.576f };
		float cascade_level{ 22.188f };
		bool enable_edit{ false };
		bool enable_sun_debug{ false };
		bool enable_light_cam{ false };
		bool reverse_winding_order_enabled{ false };
		bool enable_cull{ true };
		bool enable_wire{ false };
		bool enable_light_perspective{ false };
		bool depth_bias_enabled{ true };
		float depth_bias_constant{-17.242f };
		float depth_bias_clamp{ -114.858f };
		float depth_bias_slope_factor{ -7.376f };
	};

}
