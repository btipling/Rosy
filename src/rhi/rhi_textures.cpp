#include "rhi.h"

allocated_image_result rhi::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mip_mapped)
{

	allocated_image new_image;
	new_image.image_format = format;
	new_image.image_extent = size;

	VkImageCreateInfo img_info = img_create_info(format, usage, size);
	if (mip_mapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (VkResult result = vmaCreateImage(allocator_.value(), &img_info, &alloc_info, &new_image.image, &new_image.allocation, nullptr); result != VK_SUCCESS) {
		allocated_image_result rv = {};
		rv.result = result;
		return rv;
	}

	VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VkImageViewCreateInfo view_info = img_view_create_info(format, new_image.image, aspect_flag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	if (VkResult result = vkCreateImageView(device_.value(), &view_info, nullptr, &new_image.image_view); result != VK_SUCCESS)
	{
		allocated_image_result rv = {};
		rv.result = result;
		return rv;
	}
	{
		allocated_image_result rv = {};
		rv.result = VK_SUCCESS;
		rv.image = new_image;
		return rv;
	}
}

allocated_image_result rhi::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mip_mapped)
{
	allocated_image_result rv = {};
	rv.result = VK_SUCCESS;
	return rv;
}

void rhi::destroy_image(const allocated_image& img)
{
	
}