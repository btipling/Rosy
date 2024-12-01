#include "rhi.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "../rhi/rhi_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>



rhi_buffer::rhi_buffer(rhi* renderer) : renderer_{ renderer }
{
	// Create an rhi buffer
}


std::optional<std::vector<std::shared_ptr<mesh_asset>>> rhi_buffer::load_gltf_meshes(std::filesystem::path file_path) {
	fastgltf::Asset gltf;
	fastgltf::Parser parser{};
	auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
	if (data.error() != fastgltf::Error::None) {
		return std::nullopt;
	}
	auto asset = parser.loadGltf(data.get(), file_path.parent_path(), fastgltf::Options::None);
	if (asset) {
		gltf = std::move(asset.get());
	}
	else {
		auto err = fastgltf::to_underlying(asset.error());
		rosy_utils::debug_print_a("failed to load gltf: %d %s\n", err, file_path.string().c_str());
		return std::nullopt;
	}
	std::vector<std::shared_ptr<mesh_asset>> meshes;

	// use the same vectors for all meshes so that the memory doesn't reallocate as
	// often
	std::vector<uint32_t> indices;
	std::vector<vertex> vertices;
	for (fastgltf::Mesh& mesh : gltf.meshes) {
		mesh_asset new_mesh;

		new_mesh.name = mesh.name;

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (fastgltf::Primitive p : mesh.primitives) {
			geo_surface newSurface;
			newSurface.start_index = static_cast<uint32_t>(indices.size());
			newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

			size_t initial_vtx = vertices.size();

			// load indexes
			{
				fastgltf::Accessor& index_accessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + index_accessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor,
					[&](std::uint32_t idx) {
						indices.push_back(idx + initial_vtx);
					});

			}

			// load vertex positions
			{
				auto positionIt = p.findAttribute("POSITION");
				auto& posAccessor = gltf.accessors[positionIt->accessorIndex];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 v, size_t index) {
						vertex newvtx;
						newvtx.position = glm::vec4{ v, 1.0f };
						newvtx.normal = { 1.0f, 0.0f, 0.0f, 1.0f };
						newvtx.color = glm::vec4{ 1.f };
						newvtx.texture_coordinates = { 0.0f, 0.0f, 0.0f, 0.0f };
						vertices[initial_vtx + index] = newvtx;
					});
			}

			// load vertex normals
			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
					[&](glm::vec3 v, size_t index) {
						vertices[initial_vtx + index].normal = glm::vec4{ v, 0.0f };
					});
			}

			// load UVs
			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).accessorIndex],
					[&](glm::vec2 v, size_t index) {
						vertices[initial_vtx + index].texture_coordinates = { v.x, v.y, 0.0f, 0.0f };
					});
			}

			// load vertex colors
			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
					[&](glm::vec4 v, size_t index) {
						vertices[initial_vtx + index].color = v;
					});
			}
			new_mesh.surfaces.push_back(newSurface);
		}

		// display the vertex normals
		constexpr bool override_colors = true;
		if (override_colors) {
			for (vertex& vtx : vertices) {
				vtx.color = vtx.normal;
			}
		}
		auto result = upload_mesh(indices, vertices);
		if (result.result != VK_SUCCESS) {
			rosy_utils::debug_print_a("failed to upload mesh: %d\n", result.result);
			return std::nullopt;
		}
		new_mesh.mesh_buffers = result.buffers;
		meshes.emplace_back(std::make_shared<mesh_asset>(std::move(new_mesh)));
	}

	return meshes;
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
	vertex_buffer_address = vkGetBufferDeviceAddress(renderer_->opt_device.value(), &device_address_info);
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
	vmaMapMemory(renderer_->opt_allocator.value(), staging.allocation, &data);
	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy(static_cast<char*>(data) + vertex_buffer_size, indices.data(), index_buffer_size);
	vmaUnmapMemory(renderer_->opt_allocator.value(), staging.allocation);

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
	return rv;
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
		const VkResult result = vmaCreateBuffer(renderer_->opt_allocator.value(), &buffer_info, &vma_alloc_info, &new_buffer.buffer,
			&new_buffer.allocation,
			&new_buffer.info);
		if (result != VK_SUCCESS) return { .result = result };

		return {
			.result = VK_SUCCESS,
			.buffer = new_buffer,
		};
}

void rhi_buffer::destroy_buffer(const allocated_buffer& buffer) const
{
	vmaDestroyBuffer(renderer_->opt_allocator.value(), buffer.buffer, buffer.allocation);
}
