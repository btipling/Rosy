#include "RHI.h"

GPUMeshBuffersResult Rhi::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;

	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	AllocatedBufferResult vertexBufferResult = createBuffer(
		vertexBufferSize, 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);
	if (vertexBufferResult.result != VK_SUCCESS) {
		GPUMeshBuffersResult fail = {};
		fail.result = vertexBufferResult.result;
		return fail;
	}

	// *** SETTING VERTEX BUFFER *** //
	vertexBuffer = vertexBufferResult.buffer;
	rosy_utils::DebugPrintA("vertex buffer set!\n");

	VkBufferDeviceAddressInfo deviceAddressInfo = {};
	deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	deviceAddressInfo.buffer = vertexBuffer.buffer;

	// *** SETTING VERTEX BUFFER ADDRESS *** //
	vertexBufferAddress = vkGetBufferDeviceAddress(m_device.value(), &deviceAddressInfo);
	rosy_utils::DebugPrintA("vertex buffer address set!\n");

	AllocatedBufferResult indexBufferResult = createBuffer(
		indexBufferSize, 
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);
	if (indexBufferResult.result != VK_SUCCESS) {
		GPUMeshBuffersResult fail = {};
		fail.result = indexBufferResult.result;
		return fail;
	}

	// *** SETTING INDEX BUFFER *** //
	indexBuffer = indexBufferResult.buffer;
	rosy_utils::DebugPrintA("index buffer address set!\n");

	AllocatedBufferResult stagingBuffeResult = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (stagingBuffeResult.result != VK_SUCCESS) {
		GPUMeshBuffersResult fail = {};
		fail.result = stagingBuffeResult.result;
		return fail;
	}
	AllocatedBuffer staging = stagingBuffeResult.buffer;
	rosy_utils::DebugPrintA("staging buffer created!\n");

	void* data;
	vmaMapMemory(m_allocator.value(), staging.allocation, &data);
	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);
	vmaUnmapMemory(m_allocator.value(), staging.allocation);

	rosy_utils::DebugPrintA("staging buffer mapped!\n");

	VkResult submitResult;
	submitResult = immediateSubmit([&](VkCommandBuffer cmd) {
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
		GPUMeshBuffersResult fail = {};
		fail.result = submitResult;
		return fail;
	}
	destroyBuffer(staging);
	rosy_utils::DebugPrintA("staging buffer submitted!\n");

	GPUMeshBuffers buffers = {};
	buffers.vertexBuffer = vertexBuffer;
	buffers.vertexBufferAddress = vertexBufferAddress;
	buffers.indexBuffer = indexBuffer;
	
	GPUMeshBuffersResult rv = {};
	rv.result = VK_SUCCESS;
	rv.buffers = buffers;

	rosy_utils::DebugPrintA("done uploading mesh!\n");
	return rv;
}