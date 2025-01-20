#include "Level.h"

using namespace rosy;

result level::init(log* new_log, [[maybe_unused]] config new_cfg, camera* new_cam)
{
	l = new_log;
	cam = new_cam;
	return result::ok;
}

result level::set_asset([[maybe_unused]] rosy_packager::asset new_asset)
{
	return result::ok;
}


void level::deinit()
{
	// TODO: have things to deinit I guess.
}

