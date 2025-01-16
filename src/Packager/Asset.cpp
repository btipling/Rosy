#include "Asset.h"
#include <filesystem>
#include <iostream>

using namespace rosy_packager;


rosy::result asset::write()
{
	FILE* stream;

	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';

	if (const errno_t err = fopen_s(&stream, path.c_str(), "wb"); err != 0)
	{
		std::cerr << std::format("failed to open {}, {}", path, err) << '\n';
		return rosy::result::error;
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
			return rosy::result::error;
		}
		std::cout << std::format("wrote {} headers", res) << '\n';
	}

	{
		constexpr size_t num_sizes = 1;
		const std::array<size_t, 2> sizes{ num_positions, num_triangles };
		size_t res = fwrite(&sizes, sizeof(sizes), num_sizes, stream);
		if (res != num_sizes) {
			std::cerr << std::format("failed to write, wrote {}/{} sizes", res, num_sizes) << '\n';
			return rosy::result::error;
		}
		std::cout << std::format("wrote {} sizes", res) << '\n';
	}

	{
		size_t res = fwrite(positions.data(), sizeof(position), positions.size(), stream);
		if (res != num_positions) {
			std::cerr << std::format("failed to write, wrote {}/{} positions", res, num_positions) << '\n';
			return rosy::result::error;
		}
		std::cout << std::format("wrote {} positions", res) << '\n';
	}

	{
		size_t res = fwrite(triangles.data(), sizeof(triangle), triangles.size(), stream);
		if (res != num_triangles) {
			std::cerr << std::format("failed to write, wrote {}/{} triangles", res, num_triangles) << '\n';
			return rosy::result::error;
		}
		std::cout << std::format("wrote {} triangles", res) << '\n';
	}

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';

	return rosy::result::ok;
}

rosy::result asset::read()
{
	FILE* stream;

	{
		std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';
		if (const errno_t err = fopen_s(&stream, path.c_str(), "rb"); err != 0)
		{
			std::cerr << std::format("failed to open {}, {}", path, err) << '\n';
			return rosy::result::error;
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
			return rosy::result::error;
		}
		if (header.version != current_version)
		{
			std::cerr << std::format("failed to read, version mismatch file is version {} current version is {}", header.version, current_version) << '\n';
			return rosy::result::error;
		}
		constexpr uint32_t is_little_endian = std::endian::native == std::endian::little ? 1 : 0;
		if (header.endianness != is_little_endian)
		{
			std::cerr << std::format("failed to read, endianness mismatch file is {} system is {}", header.endianness, is_little_endian) << '\n';
			return rosy::result::error;
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
			return rosy::result::error;
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
			std::cerr << std::format("failed to write, wrote {}/{} positions", res, num_positions) << '\n';
			return rosy::result::error;
		}
		std::cout << std::format("read {} positions", res) << '\n';
	}

	{
		size_t res = fread(triangles.data(), sizeof(triangle), num_triangles, stream);
		if (res != num_triangles) {
			std::cerr << std::format("failed to write, wrote {}/{} triangles", res, num_triangles) << '\n';
			return rosy::result::error;
		}
		std::cout << std::format("read {} triangles", res) << '\n';
	}

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';
	return rosy::result::ok;
}

