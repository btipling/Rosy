#include "rhi.h"


rhi_buffer::rhi_buffer(rhi* renderer) : renderer_{ renderer }
{
	// Create an rhi buffer
}

gpu_mesh_buffers_result rhi_buffer::upload_mesh(std::span<uint32_t> indices, std::span<vertex> vertices) {
	allocated_buffer index_buffer;
	allocated_buffer vertex_buffer;
	VkDeviceAddress vertex_buffer_address;

	const size_t vertex_buffer_size = vertices.size() * sizeof(vertex);
	const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

	allocated_buffer_result vertex_buffer_result = create_buffer(
		vertex_buffer_size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	if (vertex_buffer_result.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = vertex_buffer_result.result;
		return fail;
	}

	// *** SETTING VERTEX BUFFER *** //
	vertex_buffer = vertex_buffer_result.buffer;
	rosy_utils::debug_print_a("vertex buffer set!\n");
	rhi_helpers::add_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(vertex_buffer.buffer), "vertexBuffer");

	VkBufferDeviceAddressInfo device_address_info = {};
	device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	device_address_info.buffer = vertex_buffer.buffer;

	// *** SETTING VERTEX BUFFER ADDRESS *** //
	vertex_buffer_address = vkGetBufferDeviceAddress(renderer_->device_.value(), &device_address_info);
	rosy_utils::debug_print_a("vertex buffer address set!\n");

	allocated_buffer_result index_buffer_result = create_buffer(
		index_buffer_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	if (index_buffer_result.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = index_buffer_result.result;
		return fail;
	}

	// *** SETTING INDEX BUFFER *** //
	index_buffer = index_buffer_result.buffer;
	rosy_utils::debug_print_a("index buffer address set!\n");
	rhi_helpers::add_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(index_buffer.buffer), "indexBuffer");

	allocated_buffer_result staging_buffer_result = create_buffer(vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (staging_buffer_result.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = staging_buffer_result.result;
		return fail;
	}
	allocated_buffer staging = staging_buffer_result.buffer;
	rosy_utils::debug_print_a("staging buffer created!\n");

	void* data;
	vmaMapMemory(renderer_->allocator_.value(), staging.allocation, &data);
	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy(static_cast<char*>(data) + vertex_buffer_size, indices.data(), index_buffer_size);
	vmaUnmapMemory(renderer_->allocator_.value(), staging.allocation);

	rosy_utils::debug_print_a("staging buffer mapped!\n");

	VkResult submit_result;
	submit_result = renderer_->immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertex_copy{ 0 };
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = vertex_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, vertex_buffer.buffer, 1, &vertex_copy);

		VkBufferCopy index_copy{ 0 };
		index_copy.dstOffset = 0;
		index_copy.srcOffset = vertex_buffer_size;
		index_copy.size = index_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, index_buffer.buffer, 1, &index_copy);
		});
	if (submit_result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = submit_result;
		return fail;
	}
	destroy_buffer(staging);
	rosy_utils::debug_print_a("staging buffer submitted!\n");

	gpu_mesh_buffers buffers = {};
	buffers.vertex_buffer = vertex_buffer;
	buffers.vertex_buffer_address = vertex_buffer_address;
	buffers.index_buffer = index_buffer;

	gpu_mesh_buffers_result rv = {};
	rv.result = VK_SUCCESS;
	rv.buffers = buffers;

	rosy_utils::debug_print_a("done uploading mesh!\n");
}

allocated_buffer_result rhi_buffer::create_buffer(const size_t alloc_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage)
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
		const VkResult result = vmaCreateBuffer(renderer_->allocator_.value(), &buffer_info, &vma_alloc_info, &new_buffer.buffer,
			&new_buffer.allocation,
			&new_buffer.info);
		if (result != VK_SUCCESS) return { .result = result };

		return {
			.result = VK_SUCCESS,
			.buffer = new_buffer,
		};
}

void rhi_buffer::destroy_buffer(const allocated_buffer& buffer)
{
	vmaDestroyBuffer(renderer_->allocator_.value(), buffer.buffer, buffer.allocation);
}

gpu_mesh_buffers_result rhi::upload_mesh(std::span<uint32_t> indices, std::span<vertex> vertices) {
	allocated_buffer index_buffer;
	allocated_buffer vertex_buffer;
	VkDeviceAddress vertex_buffer_address;

	const size_t vertex_buffer_size = vertices.size() * sizeof(vertex);
	const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

	allocated_buffer_result vertex_buffer_result = create_buffer(
		vertex_buffer_size, 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	if (vertex_buffer_result.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = vertex_buffer_result.result;
		return fail;
	}

	// *** SETTING VERTEX BUFFER *** //
	vertex_buffer = vertex_buffer_result.buffer;
	rosy_utils::debug_print_a("vertex buffer set!\n");
	rhi_helpers::add_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(vertex_buffer.buffer), "vertexBuffer");

	VkBufferDeviceAddressInfo device_address_info = {};
	device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	device_address_info.buffer = vertex_buffer.buffer;

	// *** SETTING VERTEX BUFFER ADDRESS *** //
	vertex_buffer_address = vkGetBufferDeviceAddress(device_.value(), &device_address_info);
	rosy_utils::debug_print_a("vertex buffer address set!\n");

	allocated_buffer_result index_buffer_result = create_buffer(
		index_buffer_size, 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	if (index_buffer_result.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = index_buffer_result.result;
		return fail;
	}

	// *** SETTING INDEX BUFFER *** //
	index_buffer = index_buffer_result.buffer;
	rosy_utils::debug_print_a("index buffer address set!\n");
	rhi_helpers::add_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(index_buffer.buffer), "indexBuffer");

	allocated_buffer_result staging_buffer_result = create_buffer(vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (staging_buffer_result.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = staging_buffer_result.result;
		return fail;
	}
	allocated_buffer staging = staging_buffer_result.buffer;
	rosy_utils::debug_print_a("staging buffer created!\n");

	void* data;
	vmaMapMemory(allocator_.value(), staging.allocation, &data);
	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy(static_cast<char*>(data) + vertex_buffer_size, indices.data(), index_buffer_size);
	vmaUnmapMemory(allocator_.value(), staging.allocation);

	rosy_utils::debug_print_a("staging buffer mapped!\n");

	VkResult submit_result;
	submit_result = immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertex_copy{ 0 };
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = vertex_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, vertex_buffer.buffer, 1, &vertex_copy);

		VkBufferCopy index_copy{ 0 };
		index_copy.dstOffset = 0;
		index_copy.srcOffset = vertex_buffer_size;
		index_copy.size = index_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, index_buffer.buffer, 1, &index_copy);
	});
	if (submit_result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = submit_result;
		return fail;
	}
	buffer.value()->destroy_buffer(staging);
	rosy_utils::debug_print_a("staging buffer submitted!\n");

	gpu_mesh_buffers buffers = {};
	buffers.vertex_buffer = vertex_buffer;
	buffers.vertex_buffer_address = vertex_buffer_address;
	buffers.index_buffer = index_buffer;
	
	gpu_mesh_buffers_result rv = {};
	rv.result = VK_SUCCESS;
	rv.buffers = buffers;

	rosy_utils::debug_print_a("done uploading mesh!\n");
	return rv;
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