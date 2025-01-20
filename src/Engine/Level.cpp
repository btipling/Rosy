#include "Level.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.inl>

using namespace rosy;

constexpr size_t max_node_stack_size = 4'096;
constexpr size_t max_stack_item_list = 16'384;


result level::init(log* new_log, [[maybe_unused]] config new_cfg, camera* new_cam)
{
	l = new_log;
	cam = new_cam;
	return result::ok;
}

struct stack_item
{
	bool root{ false };
	size_t parent_stack_item_index{ 0 };
	size_t parent_index{ 0 };
	size_t node_index{ 0 };
	glm::mat4 parent_transform{ glm::mat4{1.f}};
};

std::array<float, 16> mat4_to_array(glm::mat4 m)
{
	std::array<float, 16> rv{};
	const auto pos_r = glm::value_ptr(m);
	for (uint64_t i{ 0 }; i < 16; i++) rv[i] = pos_r[i];
	return rv;
}

result level::set_asset([[maybe_unused]] const rosy_packager::asset& new_asset)
{

	return result::ok;
}


// ReSharper disable once CppMemberFunctionMayBeStatic
void level::deinit()
{
	// TODO: have things to deinit I guess.
}

