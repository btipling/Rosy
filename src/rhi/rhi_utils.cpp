#include "RHI.h";


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageSeverityFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {
	rosy_utils::debug_print_a("Validation layer debug callback: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

void rhi::debug() {
	rosy_utils::debug_print_a("RHI Debug Data::");
	if (!instance_.has_value()) {
		rosy_utils::debug_print_a("No instance!");
		return;
	}

	if (!physical_device_properties_.has_value()) {
		rosy_utils::debug_print_a("No physical device!");
		return;
	}
	VkPhysicalDeviceProperties deviceProperties = physical_device_properties_.value();
	VkPhysicalDeviceFeatures deviceFeatures = supported_features_.value();
	VkPhysicalDeviceMemoryProperties deviceMemProps = physical_device_memory_properties_.value();
	std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData = queue_family_properties_.value();
	rosy_utils::debug_print_a("result device property vendor %s \n", deviceProperties.deviceName);
	rosy_utils::debug_print_a("result: vendor: %u \n", deviceProperties.vendorID);

	rosy_utils::debug_print_a("has multiDrawIndirect? %d \n", deviceFeatures.multiDrawIndirect);
	for (uint32_t i = 0; i < deviceMemProps.memoryHeapCount; i++) {
		rosy_utils::debug_print_a("memory size: %d\n", deviceMemProps.memoryHeaps[i].size);
		rosy_utils::debug_print_a("memory flags: %d\n", deviceMemProps.memoryHeaps[i].flags);
	}
	for (const VkQueueFamilyProperties& qfmp : queueFamilyPropertiesData) {
		rosy_utils::debug_print_a("queue count: %d and time bits: %d\n", qfmp.queueCount, qfmp.timestampValidBits);
		if (qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT)) {
			rosy_utils::debug_print_a("VkQueueFamilyProperties got all the things\n");
		}
		else {
			rosy_utils::debug_print_a("VkQueueFamilyProperties missing stuff\n");
		}
	}
	rosy_utils::debug_print_a("Selected queue index %d with count: %d\n", queue_index_, queue_count_);
}

VkDebugUtilsMessengerCreateInfoEXT createDebugCallbackInfo() {
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr;
	return createInfo;
}


std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
}

swap_chain_support_details rhi::query_swap_chain_support(VkPhysicalDevice device) {
	swap_chain_support_details details = {};
	VkSurfaceKHR surface = surface_.value();
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.present_modes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.present_modes.data());
	}
	return details;
}

VkImageCreateInfo rhi::img_create_info (VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent) {
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

VkImageViewCreateInfo rhi::img_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags) {
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

VkRenderingAttachmentInfo rhi::attachment_info(VkImageView view, VkImageLayout layout) {
	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.pNext = nullptr;
	colorAttachment.imageView = view;
	colorAttachment.imageLayout = layout;
	colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	return colorAttachment;
}

VkRenderingAttachmentInfo rhi::depth_attachment_info(VkImageView view, VkImageLayout layout) {
	VkRenderingAttachmentInfo depthAttachment = {};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.pNext = nullptr;
	depthAttachment.imageView = view;
	depthAttachment.imageLayout = layout;
	depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.clearValue.depthStencil.depth = 0.0f;
	return depthAttachment;
}

VkRenderingInfo rhi::rendering_info(VkExtent2D render_extent, VkRenderingAttachmentInfo color_attachment, std::optional<VkRenderingAttachmentInfo> depth_attachment) {
	VkRect2D renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, render_extent };
	VkRenderingInfo renderInfo = {};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.pNext = nullptr;
	renderInfo.renderArea = renderArea;
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &color_attachment;
	if (depth_attachment.has_value()) {
		renderInfo.pDepthAttachment = &depth_attachment.value();
	}
	else {
		renderInfo.pDepthAttachment = nullptr;
	}
	renderInfo.pStencilAttachment = nullptr;
	return renderInfo;
}



void rhi::blit_images(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size) {
	VkImageBlit2 blitRegion = {};
	blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blitRegion.pNext = nullptr;
	blitRegion.srcOffsets[1].x = src_size.width;
	blitRegion.srcOffsets[1].y = src_size.height;
	blitRegion.srcOffsets[1].z = 1;

	blitRegion.dstOffsets[1].x = dst_size.width;
	blitRegion.dstOffsets[1].y = dst_size.height;
	blitRegion.dstOffsets[1].z = 1;

	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;

	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blitInfo.dstImage = destination;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.srcImage = source;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;

	vkCmdBlitImage2(cmd, &blitInfo);
}

allocated_buffer_result rhi::create_buffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
	VkResult result;

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.size = alloc_size;
	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memory_usage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	allocated_buffer newBuffer;
	result = vmaCreateBuffer(allocator_.value(), &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info);
	if (result != VK_SUCCESS) return { .result = result };

	return {
		.result = VK_SUCCESS,
		.buffer = newBuffer,
	};
}

void rhi::destroy_buffer(const allocated_buffer& buffer) {
	vmaDestroyBuffer(allocator_.value(), buffer.buffer, buffer.allocation);
}

VkDebugUtilsObjectNameInfoEXT rhi::add_name(VkObjectType object_type, uint64_t object_handle, const char* p_object_name) {
	VkDebugUtilsObjectNameInfoEXT debugName = {};
	debugName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	debugName.pNext = nullptr;
	debugName.objectType = object_type;
	debugName.objectHandle = object_handle;
	debugName.pObjectName = p_object_name;
	return debugName;
}
 