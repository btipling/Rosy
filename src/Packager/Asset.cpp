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
	const size_t res = fwrite(&sizes, sizeof(sizes), 1, stream);

	std::cout << std::format("wrote {} elements", res) << '\n';

	int num_closed = _fcloseall();

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

	const size_t res1 = fread(&sizes, sizeof(sizes), 1, stream);

	std::cout << std::format("foo read {} elements", res1) << '\n';

	//std::array<char, 3> bar{};

	//const size_t res2 = fread(&bar, sizeof(bar), 1, stream);

	//std::cout << std::format("read {} elements", res2) << '\n';

	int num_closed = _fcloseall();

	std::cout << std::format("closed {} files", num_closed) << '\n';
	std::cout << std::format("result: {}", sizes) << '\n';

	return rosy::result::ok;
}

