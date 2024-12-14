// ReSharper disable CppParameterMayBeConstPtrOrRef
// ReSharper disable CppParameterMayBeConst
#include <random>
#include <unordered_map>

#include "rhi.h"

namespace ktx_sub_allocator
{

    struct allocation_info
    {
        VmaAllocation allocation;
        VkDeviceSize map_size;
    };

    namespace {
        VmaAllocator vma_allocator;
        std::mt19937_64 mt64{ std::random_device{}() };
        std::unordered_map<uint64_t, allocation_info> alloc_mem_c_wrapper_directory;
        bool is_initialized = false;
    }

	void init_vma(const VmaAllocator& allocator)
	{
        if (is_initialized) return;
		vma_allocator = allocator;
        is_initialized = true;
	}

	uint64_t alloc_mem_c_wrapper(VkMemoryAllocateInfo* alloc_info, VkMemoryRequirements* mem_req, uint64_t* num_pages)
    {
        const uint64_t alloc_id = mt64();
        VmaAllocationCreateInfo p_create_info = {};
        bool b_using_host_visible_memory = false;
        if ((alloc_info->memoryTypeIndex & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
        {
            b_using_host_visible_memory = true;
            p_create_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            p_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        }
        else
        {
            p_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            p_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        }
        p_create_info.memoryTypeBits = mem_req->memoryTypeBits;
        VmaAllocation allocation;
        if (const VkResult result = vmaAllocateMemory(vma_allocator, mem_req, &p_create_info, &allocation, nullptr); result != VK_SUCCESS)
        {
            return 0ull;
        }
        alloc_mem_c_wrapper_directory[alloc_id].allocation = allocation;
        alloc_mem_c_wrapper_directory[alloc_id].map_size = mem_req->size;
        *num_pages = 1ull;

        vmaSetAllocationName(
            vma_allocator,
            allocation,
            std::format(
                "KTX texture allocation #{} {}",
                alloc_id,
                b_using_host_visible_memory ? "(upload resource)" : "")
            .c_str());

        return alloc_id;
    }


	VkResult bind_buffer_memory_c_wrapper(VkBuffer buffer, uint64_t alloc_id)
    {
        return vmaBindBufferMemory(vma_allocator, alloc_mem_c_wrapper_directory[alloc_id].allocation, buffer);
    }


	VkResult  bind_image_memory_c_wrapper(VkImage image, uint64_t alloc_id)
    {
        return vmaBindImageMemory(vma_allocator, alloc_mem_c_wrapper_directory[alloc_id].allocation, image);
    }

	VkResult map_memory_c_wrapper(uint64_t alloc_id, uint64_t, VkDeviceSize* map_length, void** data_ptr)
    {
        *map_length = alloc_mem_c_wrapper_directory[alloc_id].map_size;
        return vmaMapMemory(vma_allocator, alloc_mem_c_wrapper_directory[alloc_id].allocation, data_ptr);
    }

	void unmap_memory_c_wrapper(uint64_t alloc_id, uint64_t)
    {
        vmaUnmapMemory(vma_allocator, alloc_mem_c_wrapper_directory[alloc_id].allocation);
    }

	void free_mem_c_wrapper(uint64_t alloc_id)
    {
        vmaFreeMemory(vma_allocator, alloc_mem_c_wrapper_directory[alloc_id].allocation);
        alloc_mem_c_wrapper_directory.erase(alloc_id);
    }

}
