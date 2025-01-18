#pragma once
#include <array>
#include <string>
#include <vector>
#include "../Engine/Types.h"

namespace rosy_packager {

	constexpr uint32_t rosy_format{ 0x52535946 };  // "RSYF"
	constexpr uint32_t current_version{ 1 };

	struct file_header {
		uint32_t magic;
		uint32_t version;
		uint32_t endianness;
	};

	struct position
	{
		std::array<float, 3> vertex{ 0.f, 0.f, 0.f };
		std::array<float, 3> normal{ 0.f, 0.f, 0.f };
	};

	struct shader
	{
		std::string path{};
		std::vector<char> source;
	};

	struct triangle
	{
		std::array<uint32_t, 3> indices{ 0, 1, 2 };
	};

	struct asset
	{
		std::string source_path{};
		std::string asset_path{};
		std::vector<position> positions;
		std::vector<triangle> triangles;
		std::vector<shader> shaders;

		rosy::result write();
		rosy::result read();
		rosy::result read_shaders();
	};
}