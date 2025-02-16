#include "Editor.h"

using namespace rosy;

result editor::init(log* new_log, [[maybe_unused]] config new_cfg)
{
	new_log->info("Hello world!");
	return result::ok;
}

void editor::deinit()
{
	
}

