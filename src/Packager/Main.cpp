#include <filesystem>
#include <iostream>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
	int is_ok = EXIT_FAILURE;
	FILE* stream;
	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';
	errno_t err = fopen_s(&stream, "lol.txt", "w");
	if (err != 0)
	{
		return is_ok;
	}
	const size_t res = fwrite("lol", 3, 1, stream);
	std::cout << std::format("wrote {} elements", res) << '\n';
	int num_closed = _fcloseall();
	std::cout << std::format("closed {} files", num_closed) << '\n';
	return 0;
}
