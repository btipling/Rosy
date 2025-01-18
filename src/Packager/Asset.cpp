#include "Asset.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

using namespace rosy_packager;


rosy::result asset::import()
{
	std::filesystem::path file_path{ source_path };
	constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember |
		fastgltf::Options::AllowDouble |
		fastgltf::Options::LoadExternalBuffers |
		fastgltf::Options::DecomposeNodeMatrices;
	fastgltf::Asset gltf;
	fastgltf::Parser parser{};
	auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
	if (data.error() != fastgltf::Error::None) {
		auto err = fastgltf::to_underlying(data.error());
		std::cerr << std::format("import - failed to open {}, {}", source_path, err) << '\n';
		return rosy::result::error;
	}
	auto asset = parser.loadGltf(data.get(), file_path.parent_path(), gltf_options);
	if (asset) {
		gltf = std::move(asset.get());
	}
	else {
		auto err = fastgltf::to_underlying(asset.error());
		std::cerr << std::format("import - failed to load {}, {}", source_path, err) << '\n';
		return rosy::result::error;
	}
	positions.clear();
	triangles.clear();

	for (fastgltf::Mesh& mesh : gltf.meshes) {
		for (size_t i = 0; i < mesh.primitives.size(); i++) {  // NOLINT(modernize-loop-convert) - mesh.primitives RAII issues.
			{
				auto position_it = mesh.primitives[i].findAttribute("POSITION");
				auto& pos_accessor = gltf.accessors[position_it->accessorIndex];

				uint32_t initial_vtx = static_cast<uint32_t>(positions.size());
				positions.resize(positions.size() + pos_accessor.count);

				{
					fastgltf::Accessor& index_accessor = gltf.accessors[mesh.primitives[i].indicesAccessor.value()];
					triangles.reserve(triangles.size() + index_accessor.count / 3);

					triangle t{};
					size_t ti{ 0 };
					fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor,
						[&](const std::uint32_t idx) {
							t.indices[ti] = idx + initial_vtx;
							ti += 1;
							if (ti > 2)
							{
								triangles.push_back(t);
								ti = 0;
							}
						});
				}

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, pos_accessor,
					[&](const fastgltf::math::fvec3& v, const size_t index) {
						position new_position{};
						new_position.vertex = { v[0], v[1], v[2] };
						new_position.normal = { 1.0f, 0.0f, 0.0f };
						positions[initial_vtx + index] = new_position;
					});

				if (auto normals = mesh.primitives[i].findAttribute("NORMAL"); normals != mesh.primitives[i].attributes.end()) {
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex],
						[&](const fastgltf::math::fvec3 n, const size_t index) {
							positions[initial_vtx + index].normal = {n[0], n[1], n[2]};
						});
				}

			}
		}
	}
	return rosy::result::ok;
}

rosy::result asset::write()
{
	FILE* stream{nullptr};

	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';

	if (const errno_t err = fopen_s(&stream, asset_path.c_str(), "wb"); err != 0)
	{
		std::cerr << std::format("failed to open {}, {}", asset_path, err) << '\n';
		return rosy::result::open_failed;
	}

	const size_t num_positions{ positions.size() };
	const size_t num_triangles{ triangles.size() };

	{
		constexpr size_t num_headers = 1;
		constexpr file_header header{
			.magic = rosy_format,
			.version = current_version,
			.endianness = std::endian::native == std::endian::little ? 1 : 0,
		};
		size_t res = fwrite(&header, sizeof(header), num_headers, stream);
		if (res != num_headers) {
			std::cerr << std::format("failed to write, wrote {}/{} headers", res, num_headers) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} headers", res) << '\n';
	}

	{
		constexpr size_t num_sizes = 1;
		const std::array<size_t, 2> sizes{ num_positions, num_triangles };
		size_t res = fwrite(&sizes, sizeof(sizes), num_sizes, stream);
		if (res != num_sizes) {
			std::cerr << std::format("failed to write, wrote {}/{} sizes", res, num_sizes) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} sizes", res) << '\n';
	}

	{
		size_t res = fwrite(positions.data(), sizeof(position), positions.size(), stream);
		if (res != num_positions) {
			std::cerr << std::format("failed to write, wrote {}/{} positions", res, num_positions) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} positions", res) << '\n';
	}

	{
		size_t res = fwrite(triangles.data(), sizeof(triangle), triangles.size(), stream);
		if (res != num_triangles) {
			std::cerr << std::format("failed to write, wrote {}/{} triangles", res, num_triangles) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} triangles", res) << '\n';
	}

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';

	return rosy::result::ok;
}

rosy::result asset::read()
{
	FILE* stream{nullptr};

	{
		std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';
		if (const errno_t err = fopen_s(&stream, asset_path.c_str(), "rb"); err != 0)
		{
			std::cerr << std::format("failed to open {}, {}", asset_path, err) << '\n';
			return rosy::result::open_failed;
		}
	}

	size_t num_positions{ 0 };
	size_t num_triangles{ 0 };

	{
		constexpr size_t num_headers = 1;
		file_header header{
			.magic = 0, 
			.version = 0,
			.endianness = 0,
		};
		size_t res = fread(&header, sizeof(header), num_headers, stream);
		if (res != num_headers) {
			std::cerr << std::format("failed to read, read {}/{} headers", res, num_headers) << '\n';
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

	{
		constexpr size_t num_sizes = 1;
		std::array<size_t, 2> sizes{ 0, 0 };
		size_t res = fread(&sizes, sizeof(sizes), num_sizes, stream);
		if (res != num_sizes) {
			std::cerr << std::format("failed to read, wrote {}/{} sizes", res, num_sizes) << '\n';
			return rosy::result::read_failed;
		}
		std::cout << std::format("read {} sizes", res) << '\n';

		num_positions = sizes[0];
		num_triangles= sizes[1];
	}

	positions.resize(num_positions);
	triangles.resize(num_triangles);

	{
		size_t res = fread(positions.data(), sizeof(position), num_positions, stream);
		if (res != num_positions) {
			std::cerr << std::format("failed to read {}/{} positions", res, num_positions) << '\n';
			return rosy::result::read_failed;
		}
		std::cout << std::format("read {} positions", res) << '\n';
	}

	{
		size_t res = fread(triangles.data(), sizeof(triangle), num_triangles, stream);
		if (res != num_triangles) {
			std::cerr << std::format("failed to read {}/{} triangles", res, num_triangles) << '\n';
			return rosy::result::read_failed;
		}
		std::cout << std::format("read {} triangles", res) << '\n';
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

