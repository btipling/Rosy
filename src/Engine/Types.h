#pragma once
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

	struct surface_graphics_data
	{
		size_t mesh_index{ 0 };
		size_t graphics_object_index{ 0 };
		size_t material_index{ 0 };
		uint32_t index_count{ 0 };
		uint32_t start_index{ 0 };
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


	struct read_level_state
	{
		std::array<float, 16> v{};
		std::array<float, 16> p{};
		std::array<float, 16> vp{};
		std::array<float, 4> cam_pos{};
	};


	struct write_level_state
	{
		float light[3];
	};

}
