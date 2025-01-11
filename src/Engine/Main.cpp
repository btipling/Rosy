#include "Main.h"

int main(int argc, char* argv[])
{
	rosy::engine engine{};
	if (const rosy::result res = engine.init(); res != rosy::result::ok) {
		engine.deinit();
		return 1;
	}
	if (const rosy::result res = engine.run(); res != rosy::result::ok) {
		engine.deinit();
		return 1;
	}
	engine.deinit();
	return 0;
}
