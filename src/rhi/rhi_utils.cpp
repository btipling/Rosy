#include "RHI.h";

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

VkImageCreateInfo rhi::img_create_info(const VkFormat format, const VkImageUsageFlags usage_flags,
                                       const VkExtent3D extent)
{
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = extent;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usage_flags;
	return info;
}

VkImageViewCreateInfo rhi::img_view_create_info(const VkFormat format, const VkImage image,
                                                const VkImageAspectFlags aspect_flags)
{
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image = image;
	info.format = format;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspect_flags;
	return info;
}

VkRenderingAttachmentInfo rhi::attachment_info(const VkImageView view, const VkImageLayout layout)
{
	VkRenderingAttachmentInfo color_attachment = {};
	color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color_attachment.pNext = nullptr;
	color_attachment.imageView = view;
	color_attachment.imageLayout = layout;
	color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	return color_attachment;
}

VkRenderingAttachmentInfo rhi::depth_attachment_info(const VkImageView view, const VkImageLayout layout)
{
	VkRenderingAttachmentInfo depth_attachment = {};
	depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depth_attachment.pNext = nullptr;
	depth_attachment.imageView = view;
	depth_attachment.imageLayout = layout;
	depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.clearValue.depthStencil.depth = 0.0f;
	return depth_attachment;
}

VkRenderingInfo rhi::rendering_info(const VkExtent2D render_extent, const VkRenderingAttachmentInfo& color_attachment,
                                    const std::optional<VkRenderingAttachmentInfo>& depth_attachment)
{
	const auto render_area = VkRect2D{VkOffset2D{0, 0}, render_extent};
	VkRenderingInfo render_info = {};
	render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	render_info.pNext = nullptr;
	render_info.renderArea = render_area;
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = 1;
	render_info.pColorAttachments = &color_attachment;
	if (depth_attachment.has_value())
	{
		render_info.pDepthAttachment = &depth_attachment.value();
	}
	else
	{
		render_info.pDepthAttachment = nullptr;
	}
	render_info.pStencilAttachment = nullptr;
	return render_info;
}


void rhi::blit_images(const VkCommandBuffer cmd, const VkImage source, const VkImage destination,
                      const VkExtent2D src_size, const VkExtent2D dst_size)
{
	VkImageBlit2 blit_region = {};
	blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blit_region.pNext = nullptr;
	blit_region.srcOffsets[1].x = static_cast<int32_t>(src_size.width);
	blit_region.srcOffsets[1].y = static_cast<int32_t>(src_size.height);
	blit_region.srcOffsets[1].z = 1;

	blit_region.dstOffsets[1].x = static_cast<int32_t>(dst_size.width);
	blit_region.dstOffsets[1].y = static_cast<int32_t>(dst_size.height);
	blit_region.dstOffsets[1].z = 1;

	blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.srcSubresource.baseArrayLayer = 0;
	blit_region.srcSubresource.layerCount = 1;
	blit_region.srcSubresource.mipLevel = 0;

	blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.dstSubresource.baseArrayLayer = 0;
	blit_region.dstSubresource.layerCount = 1;
	blit_region.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blit_info{};
	blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blit_info.pNext = nullptr;
	blit_info.dstImage = destination;
	blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blit_info.srcImage = source;
	blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blit_info.filter = VK_FILTER_LINEAR;
	blit_info.regionCount = 1;
	blit_info.pRegions = &blit_region;

	vkCmdBlitImage2(cmd, &blit_info);
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
	if (result != VK_SUCCESS) return {.result = result};

	return {
		.result = VK_SUCCESS,
		.buffer = new_buffer,
	};
}

void rhi::destroy_buffer(const allocated_buffer& buffer) const
{
	vmaDestroyBuffer(allocator_.value(), buffer.buffer, buffer.allocation);
}

VkDebugUtilsObjectNameInfoEXT rhi::add_name(const VkObjectType object_type, const uint64_t object_handle,
                                            const char* p_object_name)
{
	VkDebugUtilsObjectNameInfoEXT debug_name = {};
	debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	debug_name.pNext = nullptr;
	debug_name.objectType = object_type;
	debug_name.objectHandle = object_handle;
	debug_name.pObjectName = p_object_name;
	return debug_name;
}
