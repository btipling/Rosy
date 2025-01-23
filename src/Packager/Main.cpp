#include "Gltf.h"
#include <iostream>
#include <filesystem>

using namespace rosy_packager;

int main(const int argc, char* argv[])
{

	rosy::log l{};
#ifdef ROSY_LOG_LEVEL_DEBUG
	l.level = rosy::log_level::debug;
#endif
	const auto start = std::chrono::system_clock::now();
	l.info("Starting packager");
	l.debug(std::format("Received {} args.", argc));
	const auto cwd = std::filesystem::current_path();
	for (int i = 0; i < argc; i++)
	{
		l.debug(std::format("arg {}: {}", i, argv[i]));
	}
	if (argc <= 1)
	{
		l.error("Need to provide a relative or absolute path to a gltf file");
		return EXIT_FAILURE;
	}
	std::filesystem::path source_path{ argv[1] };
	if (!source_path.has_extension()) {
		l.error("Need to provide a path to gltf file with the gltf extension, glb is not supported.");
		return EXIT_FAILURE;
	}
	if (source_path.extension() != ".gltf")
	{
		l.error(std::format("Received a path without a gltf extension, glb is not supported. Found {}", source_path.extension().string()));
		return EXIT_FAILURE;
	}
	if (!source_path.is_absolute())
	{
		source_path = std::filesystem::path{ std::format("{}\\{}", cwd.string(), source_path.string()) };
	}
	std::filesystem::path output_path{ source_path };
	output_path.replace_extension(".rsy");
	l.info(std::format("Parsing {} as {}", source_path.string(), output_path.string()));

	gltf g{};
	{
		asset a{};
		a.asset_path = output_path.string();
		g.source_path = source_path.string();
		g.gltf_asset = a;
	}
	if (const auto res = g.import(&l); res != rosy::result::ok)
	{
		l.error(std::format("Error importing gltf {}", static_cast<uint8_t>(res)));
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
	l.debug("Parsed meshes:");
	for (auto& m : b.meshes)
	{

		for (auto& [vertex, normal, color, texture_coordinates] : m.positions)
		{
			l.debug(std::format("vertex: {} normal: {} color: {} tc: {}", vertex, normal, color, texture_coordinates));
		}
		l.debug("Parsed indices:");
		l.debug(std::format("indices: {}", m.indices));
	}
	const auto end = std::chrono::system_clock::now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	l.info(std::format("Finished packaging. Took {}ms", elapsed.count() / 1000.0l));
	return 0;
}
