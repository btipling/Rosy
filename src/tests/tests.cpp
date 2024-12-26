#include "pch.h"

#include <ranges>

#include "CppUnitTest.h"
#include "../rhi/rhi.h"
#include "../rhi/descriptor.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace unit_test
{
	TEST_CLASS(unit_test_descriptor_allocator)
	{
	public:
		
		TEST_METHOD(test_first_allocations)
		{
			constexpr uint32_t max_indices{ 1000 };
			descriptor::allocator images{ max_indices };

			for (uint32_t i = 0; i < max_indices; i++)
			{
				const std::expected<uint32_t, rhi_lib::result> result = images.allocate();
				const uint32_t expected = i;
				Assert::IsTrue(result.has_value());
				Assert::AreEqual(expected, result.value());
			}
		}

		TEST_METHOD(test_recycled_allocations)
		{
			constexpr uint32_t max_indices{ 1000 };
			descriptor::allocator images{ max_indices };

			std::expected<uint32_t, rhi_lib::result> last_allocation{0};
			for (uint32_t i = 0; i < 500; i++)
			{
				last_allocation = images.allocate();
			}

			constexpr uint32_t expected = 499;
			Assert::IsTrue(last_allocation.has_value());
			Assert::AreEqual(expected, last_allocation.value());

			images.free(last_allocation.value());
			last_allocation = images.allocate();

			Assert::IsTrue(last_allocation.has_value());
			Assert::AreEqual(expected, last_allocation.value());

		}

		TEST_METHOD(test_limit_allocations)
		{
			constexpr uint32_t max_indices{ 2 };
			descriptor::allocator images{ max_indices };

			std::expected<uint32_t, rhi_lib::result> last_allocation{ 0 };
			last_allocation = images.allocate();
			Assert::IsTrue(last_allocation.has_value());
			last_allocation = images.allocate();
			Assert::IsTrue(last_allocation.has_value());
			last_allocation = images.allocate();
			constexpr auto expected{rhi_lib::result::limit_reached};
			const rhi_lib::result result = last_allocation.error();
			Assert::IsTrue(expected == result);
		}

		TEST_METHOD(test_reset_allocations)
		{
			constexpr uint32_t max_indices{ 1000 };
			descriptor::allocator images{ max_indices };

			for (uint32_t i = 0; i < max_indices; i++)
			{
				const std::expected<uint32_t, rhi_lib::result> result = images.allocate();
				Assert::IsTrue(result.has_value());
			}
			images.reset();
			{
				constexpr uint32_t expected = 0;
				const std::expected<uint32_t, rhi_lib::result> result = images.allocate();
				Assert::IsTrue(result.has_value());
				Assert::AreEqual(expected, result.value());
			}
		}
	};
}
