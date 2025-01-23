#include "Gltf.h"
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

	rosy::log l{};

	gltf g{};
	{
		asset a{};
		a.asset_path = output_path.string();
		g.source_path = source_path.string();
		g.gltf_asset = a;
	}
	if (const auto res = g.import(&l); res != rosy::result::ok)
	{
		std::cerr << std::format("Error importing gltf {}", static_cast<uint8_t>(res)) << '\n';
		return EXIT_FAILURE;
	}
	if (const auto res = g.gltf_asset.write(&l); res != rosy::result::ok) {
		return EXIT_FAILURE;
	}

	asset b{};
	b.asset_path = output_path.string();
	if (const auto res = b.read(&l); res != rosy::result::ok) {
		return EXIT_FAILURE;
	}
	std::cout << "Parsed meshes:" << '\n';
	for (auto& m : b.meshes)
	{
		
	for (auto& [vertex, normal, color, texture_coordinates]: m.positions)
	{
		std::cout << std::format("vertex: {} normal: {} color: {} tc: {}", vertex, normal, color, texture_coordinates) << '\n';
	}
	std::cout << "Parsed indices:" << '\n';
	std::cout << std::format("indices: {}", m.indices) << '\n';
	}
	return 0;
}
