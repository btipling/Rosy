#include "Node.h"

#include <format>

using namespace rosy;

result node::init(rosy::log* new_log)
{
	l = new_log;
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
}

void node::debug()
{
	l->debug(std::format("game node name: {}", name));
	for (node* n : children)
	{
		n->debug();
	}
	children.erase(children.begin(), children.end());
}