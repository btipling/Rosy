#include "Asset.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace rosy_packager;

// Rosy File Format:
// 1. Header
// 2. GLTF Size Layouts: std::array<size_t,2> 
		// index 0 - num materials
		// index 1 - num meshes
// 3. a std::vector<material> of material size given
// 3. Per mesh:
// 3.a Mesh size layout: std::array<size_t,3>
		// index 0 - num positions -> a std::vector<position> of positions size given
	    // index 1 - num indices -> a std::vector<uint32_t> of indices size given
	    // index 2 - num surfaces -> a std::vector<surface> of surfaces size given

rosy::result asset::write()
{

	// OPEN FILE FOR WRITING BINARY

	FILE* stream{nullptr};

	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';

	if (const errno_t err = fopen_s(&stream, asset_path.c_str(), "wb"); err != 0)
	{
		std::cerr << std::format("failed to open {}, {}", asset_path, err) << '\n';
		return rosy::result::open_failed;
	}

	// WRITE RSY FORMAT HEADER

	{
		constexpr size_t num_headers = 1;
		constexpr file_header header{
			.magic = rosy_format,
			.version = current_version,
			.endianness = std::endian::native == std::endian::little ? 1 : 0,
		};
		size_t res = fwrite(&header, sizeof(header), num_headers, stream);
		if (res != num_headers) {
			std::cerr << std::format("failed to write {}/{} headers", res, num_headers) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} headers", res) << '\n';
	}

	// WRITE GLTF SIZES FOR MATERIALS AND MESHES

	const size_t num_materials{ materials.size() };
	// ReSharper disable once CppTooWideScope
	const size_t num_meshes{ meshes.size() };
	{
		constexpr size_t lookup_sizes = 1;
		const std::array<size_t, 2> sizes{ num_materials, num_meshes };
		size_t res = fwrite(&sizes, sizeof(sizes), lookup_sizes, stream);
		if (res != lookup_sizes) {
			std::cerr << std::format("failed to write {}/{} num_gltf_sizes", res, lookup_sizes) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} sizes, num_materials: {} num_meshes: {}", res, num_materials, num_meshes) << '\n';
	}

	// WRITE MATERIALS

	{
		size_t res = fwrite(materials.data(), sizeof(material), materials.size(), stream);
		if (res != num_materials) {
			std::cerr << std::format("failed to write {}/{} materials", res, num_materials) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} materials", res) << '\n';
	}


	// WRITE ALL MESHES ONE AT A TIME

	for (const auto& [positions, indices, surfaces] : meshes) {

		// WRITE ONE MESH SIZE

		const size_t num_positions{ positions.size()};
		const size_t num_indices{ indices.size() };
		const size_t num_surfaces{ surfaces.size() };
		{
			constexpr size_t lookup_sizes = 1;
			const std::array<size_t, 3> mesh_sizes{ num_positions, num_indices, num_surfaces };
			size_t res = fwrite(&mesh_sizes, sizeof(mesh_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to write {}/{} num_mesh_sizes", res, lookup_sizes) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format(
				"wrote {} sizes, num_positions: {} num_indices: {} num_surfaces: {}",
				res, num_positions, num_indices, num_surfaces) << '\n';
		}

		// WRITE ONE MESH POSITIONS

		{
			size_t res = fwrite(positions.data(), sizeof(position), positions.size(), stream);
			if (res != num_positions) {
				std::cerr << std::format("failed to write {}/{} positions", res, num_positions) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} positions", res) << '\n';
		}

		// WRITE ONE MESH INDICES

		{
			size_t res = fwrite(indices.data(), sizeof(uint32_t), indices.size(), stream);
			if (res != num_indices) {
				std::cerr << std::format("failed to write {}/{} indices", res, num_indices) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} indices", res) << '\n';
		}

		// WRITE ONE MESH SURFACES

		{
			size_t res = fwrite(surfaces.data(), sizeof(surface), surfaces.size(), stream);
			if (res != num_surfaces) {
				std::cerr << std::format("failed to write {}/{} surfaces", res, num_surfaces) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} surfaces", res) << '\n';
		}
	}

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';

	return rosy::result::ok;
}

rosy::result asset::read()
{

	// OPEN FILE FOR READING BINARY

	FILE* stream{nullptr};

	{
		std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';
		if (const errno_t err = fopen_s(&stream, asset_path.c_str(), "rb"); err != 0)
		{
			std::cerr << std::format("failed to open {}, {}", asset_path, err) << '\n';
			return rosy::result::open_failed;
		}
	}

	// READ RSY FORMAT HEADER
	{
		constexpr size_t num_headers = 1;
		file_header header{
			.magic = 0, 
			.version = 0,
			.endianness = 0,
		};
		size_t res = fread(&header, sizeof(header), num_headers, stream);
		if (res != num_headers) {
			std::cerr << std::format("failed to read {}/{} headers", res, num_headers) << '\n';
			return rosy::result::read_failed;
		}
		if (header.version != current_version)
		{
			std::cerr << std::format("failed to read, version mismatch file is version {} current version is {}", header.version, current_version) << '\n';
			return rosy::result::read_failed;
		}
		constexpr uint32_t is_little_endian = std::endian::native == std::endian::little ? 1 : 0;
		if (header.endianness != is_little_endian)
		{
			std::cerr << std::format("failed to read, endianness mismatch file is {} system is {}", header.endianness, is_little_endian) << '\n';
			return rosy::result::read_failed;
		}
		std::cout << std::format("wrote {} headers", res) << '\n';
		std::cout << std::format("format version: {} is little endian: {}", header.version, is_little_endian) << '\n';
	}

	// WRITE GLTF SIZES FOR MATERIALS AND MESHES

	size_t num_materials{ 0 };
	size_t num_meshes{ 0 };
	{
		constexpr size_t lookup_sizes = 1;
		std::array<size_t, 2> num_gltf_sizes{ 0, 0 };
		size_t res = fread(&num_gltf_sizes, sizeof(num_gltf_sizes), lookup_sizes, stream);
		if (res != lookup_sizes) {
			std::cerr << std::format("failed to read {}/{} num_gltf_sizes", res, lookup_sizes) << '\n';
			return rosy::result::read_failed;
		}
		std::cout << std::format("read {} sizes", res) << '\n';

		num_materials = num_gltf_sizes[0];
		num_meshes= num_gltf_sizes[1];
	}

	materials.resize(num_materials);
	meshes.reserve(num_meshes);

	// READ GLTF MATERIALS
	{
		size_t res = fread(materials.data(), sizeof(material), num_materials, stream);
		if (res != num_materials) {
			std::cerr << std::format("failed to read {}/{} materials", res, num_materials) << '\n';
			return rosy::result::read_failed;
		}
		std::cout << std::format("read {} positions", res) << '\n';
	}

	// READ ALL MESHES ONE AT A TIME

	for (size_t i{ 0 }; i < num_meshes; i++) {
		mesh m{};

		// READ ONE MESH SIZE

		size_t num_positions{ 0 };
		size_t num_indices{ 0 };
		size_t num_surfaces{ 0 };
		{
			constexpr size_t lookup_sizes = 1;
			std::array<size_t, 3> mesh_sizes{ 0, 0, 0 };
			size_t res = fread(&mesh_sizes, sizeof(mesh_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to read {}/{} num_mesh_sizes", res, lookup_sizes) << '\n';
				return rosy::result::read_failed;
			}
			num_positions = mesh_sizes[0];
			num_indices = mesh_sizes[1];
			num_surfaces = mesh_sizes[2];
			std::cout << std::format(
				"read {} sizes, num_positions: {} num_indices: {} num_surfaces: {}",
				res, num_positions, num_indices, num_surfaces) << '\n';
		}

		m.positions.resize(num_positions);
		m.indices.resize(num_indices);
		m.surfaces.resize(num_surfaces);

		// READ ONE MESH POSITIONS

		{
			size_t res = fread(m.positions.data(), sizeof(position), num_positions, stream);
			if (res != num_positions) {
				std::cerr << std::format("failed to read {}/{} positions", res, num_positions) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} positions", res) << '\n';
		}

		// READ ONE MESH INDICES

		{
			size_t res = fread(m.indices.data(), sizeof(uint32_t), num_indices, stream);
			if (res != num_indices) {
				std::cerr << std::format("failed to read {}/{} indices", res, num_indices) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} indices", res) << '\n';
		}

		// READ ONE MESH SURFACES

		{
			size_t res = fread(m.surfaces.data(), sizeof(surface), num_surfaces, stream);
			if (res != num_surfaces) {
				std::cerr << std::format("failed to read {}/{} surfaces", res, num_surfaces) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} surfaces", res) << '\n';
		}

		// ADD MESH TO ASSET

		meshes.push_back(m);
	}

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';
	return rosy::result::ok;
}

rosy::result asset::read_shaders() 
{
	// ReSharper disable once CppUseStructuredBinding
	for (shader & s: shaders)
	{
		std::ifstream file(s.path, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			std::cerr << std::format("failed to open shader {}", s.path) << '\n';
			return rosy::result::open_failed;
		}

		const std::streamsize file_size = file.tellg();
		if (file_size < 1)
		{
			std::cerr << std::format("invalid shader source {}", s.path) << '\n';
			return rosy::result::read_failed;
		}
		std::vector<char> buffer(file_size);
		s.source.resize(file_size);
		file.seekg(0);
		file.read(s.source.data(), file_size);
		file.close();
		if (static_cast<std::streamsize>(s.source.size()) != file_size)
		{
			std::cerr << std::format("failed to read shader source {}", s.path) << '\n';
			return rosy::result::read_failed;
		}

		std::cout << std::format("shader file size: {}", file_size) << '\n';
	}
	std::cout << std::format("read {} shaders", shaders.size()) << '\n';
	return rosy::result::ok;
}

