#pragma once
#include "Types.h"
#include "Telemetry.h"

namespace rosy {

	struct node
	{
		log* l{ nullptr };
		std::string name{};
		std::vector<node*> children;

		[[nodiscard]] auto init(log* new_log) -> result;
		void deinit();
		void debug();
	};

}