#pragma once
#include <array>
#include <format>
#include <vector>
#include "../Engine/Types.h"

namespace rosy_packager {
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