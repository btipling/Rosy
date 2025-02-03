#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <string>

// These are type declarations, not default configurations. Configure those in Level.cpp or elsewhere.
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
		std::array<float, 16> object_space_transform;
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
		float pitch{ 0.f };
		float yaw{ 0.f };
	};

	struct light_read_write_state
	{
		std::array<float, 4> sunlight{};
		bool depth_bias_enabled{ false };
		float depth_bias_constant{ 0.f };
		float depth_bias_clamp{ 0.f };
		float depth_bias_slope_factor{ 0.f };
		bool flip_light_x{ false };
		bool flip_light_y{ false };
		bool flip_light_z{ false };
		bool flip_tangent_x{ false };
		bool flip_tangent_y{ false };
		bool flip_tangent_z{ false };
		bool flip_tangent_w{ false };
	};

	struct light_debug_state
	{
		float sun_distance{ 0 };
		float sun_pitch{ 0 };
		float sun_yaw{ 0 };
		float cascade_level{ 0 };
		bool enable_light_cam{ false };
		bool enable_sun_debug{ false };
		bool enable_light_perspective{ false };
		float orthographic_depth{ 0 };
	};

	struct draw_config_state
	{
		bool reverse_winding_order_enabled{ false };
		bool cull_enabled{ false };
		bool wire_enabled{ false };
		bool thick_wire_lines{ false };
	};

	struct fragment_config_state
	{
		int output{ 0 }; // 0 normal, 1 normals, 2 tangent, 3 light
		bool light_enabled{ false };
		bool tangent_space_enabled{ false };
		bool shadows_enabled{ false };
	};

	struct scene_object
	{
		std::string name{};
		std::vector<scene_object> children;
	};

	struct read_level_state
	{
		read_camera cam{};
		light_read_write_state light{};
		draw_config_state draw_config{};
		bool debug_enabled{ false };
		std::vector<debug_object> debug_objects{};
		fragment_config_state fragment_config{};
		std::vector<scene_object> objects;
	};

	struct write_level_state
	{
		bool enable_edit{ false };
		light_read_write_state light{};
		light_debug_state light_debug{};
		draw_config_state draw_config{};
		fragment_config_state fragment_config{};
	};

}
