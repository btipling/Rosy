#include "Asset.h"

using namespace rosy_packager;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
	asset a{};
	a.path = "lol.txt";
	if (const auto res = a.write(); res != rosy::result::ok) {
		return EXIT_FAILURE;
	}
	if (const auto res = a.read(); res != rosy::result::ok) {
		return EXIT_FAILURE;
	}
	return 0;
}
