#include "RHI.h"

gpu_mesh_buffers_result rhi::upload_mesh(std::span<uint32_t> indices, std::span<vertex> vertices) {
	allocated_buffer indexBuffer;
	allocated_buffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;

	const size_t vertexBufferSize = vertices.size() * sizeof(vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	allocated_buffer_result vertexBufferResult = create_buffer(
		vertexBufferSize, 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);
	if (vertexBufferResult.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = vertexBufferResult.result;
		return fail;
	}

	// *** SETTING VERTEX BUFFER *** //
	vertexBuffer = vertexBufferResult.buffer;
	rosy_utils::debug_print_a("vertex buffer set!\n");
	add_name(VK_OBJECT_TYPE_BUFFER, (uint64_t)vertexBuffer.buffer, "vertexBuffer");

	VkBufferDeviceAddressInfo deviceAddressInfo = {};
	deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	deviceAddressInfo.buffer = vertexBuffer.buffer;

	// *** SETTING VERTEX BUFFER ADDRESS *** //
	vertexBufferAddress = vkGetBufferDeviceAddress(device_.value(), &deviceAddressInfo);
	rosy_utils::debug_print_a("vertex buffer address set!\n");

	allocated_buffer_result indexBufferResult = create_buffer(
		indexBufferSize, 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);
	if (indexBufferResult.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = indexBufferResult.result;
		return fail;
	}

	// *** SETTING INDEX BUFFER *** //
	indexBuffer = indexBufferResult.buffer;
	rosy_utils::debug_print_a("index buffer address set!\n");
	add_name(VK_OBJECT_TYPE_BUFFER, (uint64_t)indexBuffer.buffer, "indexBuffer");

	allocated_buffer_result stagingBuffeResult = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (stagingBuffeResult.result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = stagingBuffeResult.result;
		return fail;
	}
	allocated_buffer staging = stagingBuffeResult.buffer;
	rosy_utils::debug_print_a("staging buffer created!\n");

	void* data;
	vmaMapMemory(allocator_.value(), staging.allocation, &data);
	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);
	vmaUnmapMemory(allocator_.value(), staging.allocation);

	rosy_utils::debug_print_a("staging buffer mapped!\n");

	VkResult submitResult;
	submitResult = immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, indexBuffer.buffer, 1, &indexCopy);
	});
	if (submitResult != VK_SUCCESS) {
		gpu_mesh_buffers_result fail = {};
		fail.result = submitResult;
		return fail;
	}
	destroy_buffer(staging);
	rosy_utils::debug_print_a("staging buffer submitted!\n");

	gpu_mesh_buffers buffers = {};
	buffers.vertex_buffer = vertexBuffer;
	buffers.vertex_buffer_address = vertexBufferAddress;
	buffers.index_buffer = indexBuffer;
	
	gpu_mesh_buffers_result rv = {};
	rv.result = VK_SUCCESS;
	rv.buffers = buffers;

	rosy_utils::debug_print_a("done uploading mesh!\n");
	return rv;
}