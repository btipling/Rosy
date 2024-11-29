#include "rhi.h"
#include <fstream>

namespace
{
	VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		VkDebugUtilsMessageSeverityFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
		void* p_user_data)
	{
		rosy_utils::debug_print_a("Validation layer debug callback: %s\n", p_callback_data->pMessage);
		return VK_FALSE;
	}
}

std::vector<char> read_file(const std::string& filename)
{
	// NOLINT(misc-use-internal-linkage)
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file!");
	}

	const size_t file_size = file.tellg();
	std::vector<char> buffer(file_size);
	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();
	return buffer;
}

void rhi::debug() const
{
	rosy_utils::debug_print_a("RHI Debug Data::");
	if (!instance_.has_value())
	{
		rosy_utils::debug_print_a("No instance!");
		return;
	}

	if (!physical_device_properties_.has_value())
	{
		rosy_utils::debug_print_a("No physical device!");
		return;
	}
	VkPhysicalDeviceProperties device_properties = physical_device_properties_.value();
	const VkPhysicalDeviceFeatures device_features = supported_features_.value();
	const VkPhysicalDeviceMemoryProperties device_mem_props = physical_device_memory_properties_.value();
	const std::vector<VkQueueFamilyProperties> queue_family_properties_data = queue_family_properties_.value();
	rosy_utils::debug_print_a("result device property vendor %s \n", device_properties.deviceName);
	rosy_utils::debug_print_a("result: vendor: %u \n", device_properties.vendorID);

	rosy_utils::debug_print_a("has multiDrawIndirect? %d \n", device_features.multiDrawIndirect);
	for (uint32_t i = 0; i < device_mem_props.memoryHeapCount; i++)
	{
		rosy_utils::debug_print_a("memory size: %d\n", device_mem_props.memoryHeaps[i].size);
		rosy_utils::debug_print_a("memory flags: %d\n", device_mem_props.memoryHeaps[i].flags);
	}
	for (const VkQueueFamilyProperties& queue_family_props : queue_family_properties_data)
	{
		rosy_utils::debug_print_a("queue count: %d and time bits: %d\n", queue_family_props.queueCount,
			queue_family_props.timestampValidBits);
		if (queue_family_props.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT |
			VK_QUEUE_SPARSE_BINDING_BIT))
		{
			rosy_utils::debug_print_a("VkQueueFamilyProperties got all the things\n");
		}
		else
		{
			rosy_utils::debug_print_a("VkQueueFamilyProperties missing stuff\n");
		}
	}
	rosy_utils::debug_print_a("Selected queue index %d with count: %d\n", queue_index_, queue_count_);
}

VkDebugUtilsMessengerCreateInfoEXT create_debug_callback_info()
{
	// NOLINT(misc-use-internal-linkage)
	VkDebugUtilsMessengerCreateInfoEXT create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debug_callback;
	create_info.pUserData = nullptr;
	return create_info;
}

swap_chain_support_details rhi::query_swap_chain_support(const VkPhysicalDevice device) const
{
	swap_chain_support_details details = {};
	const VkSurfaceKHR surface = surface_.value();
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

	if (format_count != 0)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
	}
	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

	if (present_mode_count != 0)
	{
		details.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
	}
	return details;
}


allocated_buffer_result rhi::create_buffer(const size_t alloc_size, const VkBufferUsageFlags usage,
	const VmaMemoryUsage memory_usage) const
{
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.pNext = nullptr;
	buffer_info.size = alloc_size;
	buffer_info.usage = usage;

	VmaAllocationCreateInfo vma_alloc_info = {};
	vma_alloc_info.usage = memory_usage;
	vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

	allocated_buffer new_buffer;
	const VkResult result = vmaCreateBuffer(allocator_.value(), &buffer_info, &vma_alloc_info, &new_buffer.buffer,
		&new_buffer.allocation,
		&new_buffer.info);
	if (result != VK_SUCCESS) return { .result = result };

	return {
		.result = VK_SUCCESS,
		.buffer = new_buffer,
	};
}

void rhi::destroy_buffer(const allocated_buffer& buffer) const
{
	vmaDestroyBuffer(allocator_.value(), buffer.buffer, buffer.allocation);
}
