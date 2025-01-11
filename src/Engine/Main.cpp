#include "Engine.h"
#pragma warning(disable:4100)

using namespace rosy;

int main(int argc, char* argv[])
{
	rosy::engine engine{};
	if (const result res = engine.init(); res != result::ok) {
		engine.deinit();
		return 1;
	}
	if (const result res = engine.run(); res != result::ok) {
		engine.deinit();
		return 1;
	}
	engine.deinit();
	return 0;
}
