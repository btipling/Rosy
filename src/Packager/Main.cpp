#include "Asset.h"
#include <iostream>
#include <filesystem>

using namespace rosy_packager;

int main(const int argc, char* argv[])
{
	std::cout << std::format("Received {} args.", argc) << '\n';
	const auto cwd = std::filesystem::current_path();
	for (int i = 0; i < argc; i++)
	{
		std::cout << std::format("arg {}: {}", i, argv[i]) << '\n';
	}
	if (argc <= 1)
	{
		std::cerr << "Need to provide a relative or absolute path to a gltf file" << '\n';
		return EXIT_FAILURE;
	}
	std::filesystem::path source_path{ argv[1] };
	if (!source_path.has_extension()) {
		std::cerr << "Need to provide a path to gltf file with the gltf extension, glb is not supported." << '\n';
		return EXIT_FAILURE;
	}
	if (source_path.extension() != ".gltf")
	{
		std::cerr << std::format("Received a path without a gltf extension, glb is not supported. Found {}", source_path.extension().string()) << '\n';
		return EXIT_FAILURE;
	}
	if (!source_path.is_absolute())
	{
		source_path = std::filesystem::path{ std::format("{}\\{}", cwd.string(), source_path.string()) };
	}
	std::filesystem::path output_path{ source_path };
	output_path.replace_extension(".rsy");
	std::cout << std::format("Parsing {} as {}", source_path.string(), output_path.string()) << '\n';

	constexpr position p1{
		.vertex = {-0.5f, 0.f, 0.f},
		.normal = {1.f, 0.f, 0.f},
	};

	constexpr position p2{
		.vertex = {0.f, -0.5f, 0.f},
		.normal = {0.f, 1.f, 0.f},
	};

	constexpr position p3{
		.vertex = {0.5f, 0.f, 0.f},
		.normal = {0.f, 0.f, 1.f},
	};

	constexpr triangle t1{
		.indices = {0, 1, 2},
	};

	asset a{};
	a.source_path = source_path.string();
	a.asset_path = "triangle.rsy";
	a.positions = { p1, p2, p3 };
	a.triangles = { t1 };
	if (const auto res = a.write(); res != rosy::result::ok) {
		return EXIT_FAILURE;
	}

	asset b{};
	b.asset_path = "triangle.rsy";
	if (const auto res = b.read(); res != rosy::result::ok) {
		return EXIT_FAILURE;
	}
	std::cout << "Parsed positions:" << '\n';
	for (auto& [vertex, normal]: b.positions)
	{
		std::cout << std::format("vertex: {} normal: {}", vertex, normal) << '\n';
	}
	std::cout << "Parsed triangles:" << '\n';
	for (auto& [indices] : b.triangles)
	{
		std::cout << std::format("indices: {}", indices) << '\n';
	}
	return 0;
}
