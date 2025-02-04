#pragma once
#include "Types.h"
#include "Telemetry.h"

struct node_state;

namespace rosy {

	struct node
	{
		log* l{ nullptr };
		node_state* ns{ nullptr };
		std::vector<graphics_object> graphics_objects;
		std::string name{};
		std::vector<node*> children;
		std::array<float, 16> parent_transform;
		std::array<float, 16> transform;
		std::array<float, 4> position;

		[[nodiscard]] auto init(log* new_log, const std::array<float, 16>& new_transform, const std::array<float, 16>& new_parent_transform) -> result;
		void deinit();
		[[nodiscard]] auto set_position(const std::array<float, 3>& new_position) -> result;
		void update_parent_transform(const std::array<float, 16>& new_parent_transform);
		void debug();
	};

}