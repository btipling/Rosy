#include "Asset.h"
#include <filesystem>
#include <iostream>

using namespace rosy_packager;


rosy::result asset::write()
{
	FILE* stream;
	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';
	errno_t err = fopen_s(&stream, "lol.txt", "w");
	if (err != 0)
	{
		return rosy::result::error;
	}
	const size_t res = fwrite("foobar", 6, 1, stream);
	std::cout << std::format("wrote {} elements", res) << '\n';
	int num_closed = _fcloseall();
	std::cout << std::format("closed {} files", num_closed) << '\n';
	return rosy::result::ok;
}

rosy::result asset::read()
{
	FILE* stream;
	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';
	errno_t err = fopen_s(&stream, "lol.txt", "r");
	if (err != 0)
	{
		return rosy::result::error;
	}
	std::array<char, 3> foo{};
	const size_t res1 = fread(&foo, sizeof(foo), 1, stream);
	std::cout << std::format("foo read {} elements", res1) << '\n';
	std::array<char, 3> bar{};
	const size_t res2 = fread(&bar, sizeof(bar), 1, stream);
	std::cout << std::format("read {} elements", res2) << '\n';
	int num_closed = _fcloseall();
	std::cout << std::format("closed {} files", num_closed) << '\n';
	std::cout << std::format("result: {} {}", foo, bar) << '\n';
	return rosy::result::ok;
}

