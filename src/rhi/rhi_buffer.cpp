#include "RHI.h"

GPUMeshBuffersResult Rhi::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
	GPUMeshBuffersResult rv = {};

	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	auto bufferResult = createBuffer(
		vertexBufferSize, 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);
	if (bufferResult.result != VK_SUCCESS) {
		rv.result = bufferResult.result;
		return rv;
	}
	rv.buffers.vertexBuffer = bufferResult.buffer;

	VkBufferDeviceAddressInfo deviceAddressInfo = {};
	deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	deviceAddressInfo.buffer = rv.buffers.vertexBuffer.buffer;

	rv.buffers.vertexBufferAddress = vkGetBufferDeviceAddress(m_device.value(), &deviceAddressInfo);

	bufferResult = createBuffer(
		indexBufferSize, 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);
	if (bufferResult.result != VK_SUCCESS) {
		rv.result = bufferResult.result;
		return rv;
	}

	rv.buffers.indexBuffer = bufferResult.buffer;

	bufferResult = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (bufferResult.result != VK_SUCCESS) {
		rv.result = bufferResult.result;
		return rv;
	}
	AllocatedBuffer staging = bufferResult.buffer;

	void* data;
	vmaMapMemory(m_allocator.value(), staging.allocation, &data);
	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);
	vmaUnmapMemory(m_allocator.value(), staging.allocation);

	VkResult result;
	result = immediateSubmit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, rv.buffers.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, rv.buffers.indexBuffer.buffer, 1, &indexCopy);
	});
	if (bufferResult.result != VK_SUCCESS) {
		rv.result = bufferResult.result;
		return rv;
	}
	destroyBuffer(staging);

	rv.result = VK_SUCCESS;
	return rv;
}