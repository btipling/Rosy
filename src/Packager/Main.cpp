#include "Asset.h"
#include <iostream>

using namespace rosy_packager;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
	constexpr position p1{
		.vertex = {-0.5f, 0.f, 0.f},
		.normal = {1.f, 0.f, 0.f},
	};

	constexpr position p2{
		.vertex = {0.f, -0.5f, 0.f},
		.normal = {1.f, 0.f, 0.f},
	};

	constexpr position p3{
		.vertex = {0.5f, 0.f, 0.f},
		.normal = {1.f, 0.f, 0.f},
	};

	constexpr triangle t1{
		.indexes = {0, 1, 2},
	};

	asset a{};
	a.path = "triangle.rsy";
	a.positions = { p1, p2, p3 };
	a.triangles = { t1 };
	if (const auto res = a.write(); res != rosy::result::ok) {
		return EXIT_FAILURE;
	}

	asset b{};
	b.path = "triangle.rsy";
	if (const auto res = b.read(); res != rosy::result::ok) {
		return EXIT_FAILURE;
	}
	std::cout << "Parsed positions:" << '\n';
	for (auto [vertex, normal]: b.positions)
	{
		std::cout << std::format("vertex: {} normal: {}", vertex, normal) << '\n';
	}
	std::cout << "Parsed triangles:" << '\n';
	for (auto [indexes] : b.triangles)
	{
		std::cout << std::format("indexes: {}", indexes) << '\n';
	}
	return 0;
}
