#include "Engine.h"


int main(int argc, char* argv[])
{
	std::cout << "Hello World!\n";
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


namespace rosy
{
	//// Engine
	result engine::init()
	{
		std::cout << "Engine init!\n";
		return result::ok;
	}

	result engine::run()
	{
		std::cout << "Engine run!\n";
		return result::ok;
	}

	void engine::deinit()
	{
		std::cout << "Engine deinit!\n";
	}

}