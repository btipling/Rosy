#include "rhi.h"

allocated_image_result rhi::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                         bool mip_mapped) const
{
	allocated_image new_image;
	new_image.image_format = format;
	new_image.image_extent = size;

	VkImageCreateInfo img_info = rhi_helpers::img_create_info(format, usage, size);
	if (mip_mapped)
	{
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (VkResult result = vmaCreateImage(allocator_.value(), &img_info, &alloc_info, &new_image.image,
	                                     &new_image.allocation, nullptr); result != VK_SUCCESS)
	{
		allocated_image_result rv = {};
		rv.result = result;
		return rv;
	}

	VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT)
	{
		aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VkImageViewCreateInfo view_info = rhi_helpers::img_view_create_info(format, new_image.image, aspect_flag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	if (VkResult result = vkCreateImageView(device_.value(), &view_info, nullptr, &new_image.image_view); result !=
		VK_SUCCESS)
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

allocated_image_result rhi::create_image(const void* data, const VkExtent3D size, const VkFormat format,
                                         const VkImageUsageFlags usage, const bool mip_mapped)
{
	const size_t data_size = static_cast<size_t>(size.depth) * size.width * size.height * 4;
	auto [result, created_buffer] = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST  );
	if (result != VK_SUCCESS)
	{
		allocated_image_result rv = {};
		rv.result = result;
		return rv;
	}
	allocated_buffer staging = created_buffer;

	void* staging_data;
	vmaMapMemory(allocator_.value(), staging.allocation, &staging_data);
	memcpy(static_cast<char*>(staging_data), data, data_size);
	vmaUnmapMemory(allocator_.value(), staging.allocation);

	const auto image_result = create_image(size, format,
	                                       usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	                                       mip_mapped);
	if (image_result.result != VK_SUCCESS)
	{
		return image_result;
	}
	const allocated_image new_image = image_result.image;

	immediate_submit([&](const VkCommandBuffer cmd)
	{
		transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copy_region = {};
		copy_region.bufferOffset = 0;
		copy_region.bufferRowLength = 0;
		copy_region.bufferImageHeight = 0;
		copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.imageSubresource.mipLevel = 0;
		copy_region.imageSubresource.baseArrayLayer = 0;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageExtent = size;

		vkCmdCopyBufferToImage(cmd, staging.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
		                       &copy_region);

		transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});

	buffer.value()->destroy_buffer(staging);
	{
		allocated_image_result rv = {};
		rv.result = VK_SUCCESS;
		rv.image = new_image;
		return rv;
	}
}

void rhi::destroy_image(const allocated_image& img) const
{
	vkDestroyImageView(device_.value(), img.image_view, nullptr);
	vmaDestroyImage(allocator_.value(), img.image, img.allocation);
}
