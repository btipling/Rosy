#pragma once
#include "rhi.h"
#include <cstdint>
#include <expected>
#include <stack>

namespace descriptor
{
	class allocator
	{
	public:
		explicit allocator(uint32_t max_indexes);
		std::expected<uint32_t,rhi_lib::result> allocate();
		void free(uint32_t index);
	private:
		std::stack<uint32_t> recycled_indexes_;
		uint32_t max_indexes_{ 0 };
		uint32_t num_allocated_{ 0 };
	};

	class set
	{
	public:
		uint32_t binding{ 0 };
		allocator allocator;
		explicit set(uint32_t max_indexes, uint32_t binding);
	};
}
