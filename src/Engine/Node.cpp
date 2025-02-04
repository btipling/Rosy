#include "Node.h"
#include <format>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/quaternion.hpp>

using namespace rosy;

namespace
{
	std::array<float, 16> mat4_to_array(glm::mat4 m)
	{
		std::array<float, 16> a{};
		const auto pos_r = glm::value_ptr(m);
		for (uint64_t i{ 0 }; i < 16; i++) a[i] = pos_r[i];
		return a;
	}

	glm::mat4 array_to_mat4(const std::array<float, 16>& a)
	{
		glm::mat4 m{};
		const auto pos_r = glm::value_ptr(m);
		for (uint64_t i{ 0 }; i < 16; i++) pos_r[i] = a[i];
		return m;
	}

	std::array<float, 4> vec4_to_array(glm::vec4 v)
	{
		std::array<float, 4> a{};
		const auto pos_r = glm::value_ptr(v);
		for (uint64_t i{ 0 }; i < 4; i++) a[i] = pos_r[i];
		return a;
	}
}

struct node_state
{
	glm::mat4 transform{};
	glm::vec4 position{};

	void set_transform(const std::array<float, 16>& new_transform)
	{
		transform = array_to_mat4(new_transform);
		position = transform * glm::vec4(0.f, 0.f, 0.f, 1.f);
	}

	void set_position(const std::array<float, 3>& new_position)
	{
		position = glm::vec4(new_position[0], new_position[1], new_position[2], 1.f);
		transform = glm::mat4(
			transform[0],
			transform[1],
			transform[2],
			position
		);
	}
};

result node::init(rosy::log* new_log, const std::array<float, 16>& new_transform)
{
	l = new_log;
	ns = new(std::nothrow) node_state;
	if (ns == nullptr)
	{
		l->error("Error allocating node state");
		return result::allocation_failure;
	}
	ns->set_transform(new_transform);
	transform = mat4_to_array(ns->transform);
	position = vec4_to_array(ns->position);
	return result::ok;
}

void node::deinit()
{
	for (node* n : children)
	{
		n->deinit();
		delete n;
	}
	children.erase(children.begin(), children.end());
	delete ns;
	ns = nullptr;
}

auto node::set_position(const std::array<float, 3>& new_position) -> result
{
	ns->set_position(new_position);
	position = vec4_to_array(ns->position);
	return result::ok;
}

void node::debug()
{
	size_t num_surfaces{ 0 };
	for (const auto& go : graphics_objects) num_surfaces += go.surface_data.size();
	l->debug(std::format("game node name: {} num graphic objects: {} num surfaces: {}", name, graphics_objects.size(), num_surfaces));
	for (node* n : children)
	{
		n->debug();
	}
}
