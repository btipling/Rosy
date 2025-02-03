#pragma once
#include "Types.h"
#include "Telemetry.h"

struct node_state;

namespace rosy {

	struct node
	{
		log* l{ nullptr };
		node_state* ns{ nullptr };
		std::string name{};
		std::vector<node*> children;
		std::array<float, 4> position;

		[[nodiscard]] auto init(log* new_log, const std::array<float, 16>& transform) -> result;
		void deinit();
		void debug();

	};

}