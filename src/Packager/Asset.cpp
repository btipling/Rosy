#include "Asset.h"
#include <filesystem>
#include <iostream>

using namespace rosy_packager;


rosy::result asset::write()
{
	FILE* stream;

	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';

	if (const errno_t err = fopen_s(&stream, path.c_str(), "w"); err != 0)
	{
		return rosy::result::error;
	}

	const std::array<size_t, 2> sizes{positions.size(), triangles.size()};
	size_t res = fwrite(&sizes, sizeof(sizes), 1, stream);
	std::cout << std::format("wrote {} sizes", res) << '\n';
	res = fwrite(positions.data(), sizeof(position), positions.size(), stream);
	std::cout << std::format("wrote {} positions", res) << '\n';
	res = fwrite(triangles.data(), sizeof(triangle), triangles.size(), stream);
	std::cout << std::format("wrote {} triangles", res) << '\n';

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';

	return rosy::result::ok;
}

rosy::result asset::read()
{
	FILE* stream;

	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';

	if (const errno_t err = fopen_s(&stream, path.c_str(), "r"); err != 0)
	{
		return rosy::result::error;
	}

	std::array<size_t, 2> sizes{0, 0};

	size_t res = fread(&sizes, sizeof(sizes), 1, stream);
	std::cout << std::format("read {} sizes", res) << '\n';
	positions.resize(sizes[0]);
	triangles.resize(sizes[1]);
	res = fread(positions.data(), sizeof(position), sizes[0], stream);
	std::cout << std::format("read {} positions", res) << '\n';
	res = fread(triangles.data(), sizeof(triangle), sizes[1], stream);
	std::cout << std::format("read {} triangles", res) << '\n';

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';
	std::cout << std::format("result: {}", sizes) << '\n';

	return rosy::result::ok;
}

