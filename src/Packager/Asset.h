#pragma once
#include <array>
#include <format>
#include <vector>
#include "../Engine/Types.h"

namespace rosy_packager {

	constexpr uint32_t rosy_format{ 0x52535946 };  // "RSYF" in hex
	constexpr uint32_t current_version{ 1 };

	struct file_header {
		uint32_t magic;      // Magic number to identify the format
		uint32_t version;    // Format version
		uint32_t endianness; // Storage endianness
	};

	struct position
	{
		std::array<float, 3> vertex{ 0.f, 0.f, 0.f };
		std::array<float, 3> normal{ 0.f, 0.f, 0.f };
	};

	struct triangle
	{
		std::array<uint32_t, 3> indices{ 0, 1, 2 };
	};

	struct asset
	{
		std::string path{"."};
		std::vector<position> positions;
		std::vector<triangle> triangles;

		rosy::result write();
		rosy::result read();
	};
}