#include "pch.h"
#include "descriptor.h"

namespace descriptor
{
	allocator::allocator(const uint32_t max_indexes) : max_indexes{ max_indexes }
	{
	}

	std::expected<uint32_t, rhi_lib::result> allocator::allocate()
	{
		uint32_t new_index = num_allocated_;
		if (recycled_indexes_.empty())
		{
			if (num_allocated_ >= max_indexes)
				return std::unexpected(rhi_lib::result::limit_reached);
			num_allocated_ += 1;
		} else
		{
			new_index = recycled_indexes_.top();
			recycled_indexes_.pop();
		}
		return new_index;
	}

	void allocator::free(const uint32_t index)
	{
		recycled_indexes_.push(index);
	}

	void allocator::reset()
	{
		while (!recycled_indexes_.empty()) recycled_indexes_.pop();
		num_allocated_ = 0;
	}

	set::set(const uint32_t max_indexes, const uint32_t binding) : binding{ binding }, allocator(max_indexes)
	{
	}
}
