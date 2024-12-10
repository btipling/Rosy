#include "rhi.h"
#include "../rhi/rhi_types.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

rhi_data::rhi_data(rhi* renderer) : renderer_{ renderer } {}

std::optional<mesh_scene> rhi_data::load_gltf_meshes(std::filesystem::path file_path) const
{
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
	mesh_scene gltf_mesh_scene{};

	std::vector<uint32_t> indices;
	std::vector<vertex> vertices;
	for (fastgltf::Mesh& mesh : gltf.meshes) {
		mesh_asset new_mesh;

		new_mesh.name = mesh.name;

		indices.clear();
		vertices.clear();

		for (fastgltf::Primitive p : mesh.primitives) {
			geo_surface new_surface{};
			new_surface.start_index = static_cast<uint32_t>(indices.size());
			new_surface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

			size_t initial_vtx = vertices.size();

			{
				fastgltf::Accessor& index_accessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + index_accessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor,
					[&](const std::uint32_t idx) {
						indices.push_back(idx + initial_vtx);
					});

			}

			{
				auto position_it = p.findAttribute("POSITION");
				auto& pos_accessor = gltf.accessors[position_it->accessorIndex];
				vertices.resize(vertices.size() + pos_accessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, pos_accessor,
					[&](const glm::vec3 v, const size_t index) {
						vertex new_vtx{};
						new_vtx.position = v;
						new_vtx.normal = { 1.0f, 0.0f, 0.0f };
						new_vtx.color = glm::vec4{ 1.f };
						new_vtx.texture_coordinates_s = 0.0f;
						new_vtx.texture_coordinates_t = 0.0f;
						vertices[initial_vtx + index] = new_vtx;
					});
			}

			if (auto normals = p.findAttribute("NORMAL"); normals != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
					[&](const glm::vec3 v, const size_t index) {
						vertices[initial_vtx + index].normal = glm::vec4{ v, 0.0f };
					});
			}

			// ReSharper disable once StringLiteralTypo
			if (auto uv = p.findAttribute("TEXCOORD_0"); uv != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
					[&](const glm::vec2 tc, const size_t index) {
						vertices[initial_vtx + index].texture_coordinates_s = tc.x;
						vertices[initial_vtx + index].texture_coordinates_t = tc.y;
					});
			}

			if (auto colors = p.findAttribute("COLOR_0"); colors != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
					[&](const glm::vec4 v, const size_t index) {
						vertices[initial_vtx + index].color = v;
					});
			}
			new_mesh.surfaces.push_back(new_surface);
		}

		if (constexpr bool override_colors = true) {
			for (vertex& vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.0);
			}
		}
		auto [result, uploaded_mesh] = upload_mesh(indices, vertices);
		if (result != VK_SUCCESS) {
			rosy_utils::debug_print_a("failed to upload mesh: %d\n", result);
			return std::nullopt;
		}
		new_mesh.mesh_buffers = uploaded_mesh;
		gltf_mesh_scene.meshes.emplace_back(std::make_shared<mesh_asset>(std::move(new_mesh)));
	}

	return gltf_mesh_scene;
}


gpu_mesh_buffers_result rhi_data::upload_mesh(std::span<uint32_t> indices, std::span<vertex> vertices) const
{
	allocated_buffer index_buffer;
	allocated_buffer vertex_buffer;
	VkDeviceAddress vertex_buffer_address;

	const size_t vertex_buffer_size = vertices.size() * sizeof(vertex);
	const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

	auto [vertex_result, new_vertex_buffer] = create_buffer(
		"vertexBuffer",
		vertex_buffer_size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	if (vertex_result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail{};
		fail.result = vertex_result;
		return fail;
	}

	// *** SETTING VERTEX BUFFER *** //
	vertex_buffer = new_vertex_buffer;
	rosy_utils::debug_print_a("vertex buffer set!\n");

	VkBufferDeviceAddressInfo device_address_info{};
	device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	device_address_info.buffer = vertex_buffer.buffer;

	// *** SETTING VERTEX BUFFER ADDRESS *** //
	vertex_buffer_address = vkGetBufferDeviceAddress(renderer_->opt_device.value(), &device_address_info);
	rosy_utils::debug_print_a("vertex buffer address set!\n");

	auto [index_result, new_index_buffer] = create_buffer(
		"indexBuffer",
		index_buffer_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	if (index_result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail{};
		fail.result = index_result;
		return fail;
	}

	// *** SETTING INDEX BUFFER *** //
	index_buffer = new_index_buffer;
	rosy_utils::debug_print_a("index buffer address set!\n");

	auto [result, new_staging_buffer] = create_buffer("staging", vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail{};
		fail.result = result;
		return fail;
	}
	allocated_buffer staging = new_staging_buffer;
	rosy_utils::debug_print_a("staging buffer created!\n");

	void* data;
	vmaMapMemory(renderer_->opt_allocator.value(), staging.allocation, &data);
	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy(static_cast<char*>(data) + vertex_buffer_size, indices.data(), index_buffer_size);
	vmaUnmapMemory(renderer_->opt_allocator.value(), staging.allocation);

	rosy_utils::debug_print_a("staging buffer mapped!\n");

	VkResult submit_result;
	submit_result = renderer_->immediate_submit([&](const VkCommandBuffer cmd) {
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
		gpu_mesh_buffers_result fail{};
		fail.result = submit_result;
		return fail;
	}
	destroy_buffer(staging);
	rosy_utils::debug_print_a("staging buffer submitted!\n");

	gpu_mesh_buffers buffers{};
	buffers.vertex_buffer = vertex_buffer;
	buffers.vertex_buffer_address = vertex_buffer_address;
	buffers.index_buffer = index_buffer;

	gpu_mesh_buffers_result rv{};
	rv.result = VK_SUCCESS;
	rv.buffers = buffers;

	rosy_utils::debug_print_a("done uploading mesh!\n");
	return rv;
}

allocated_buffer_result rhi_data::create_buffer(const char* name, const size_t alloc_size, const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage) const
{
		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.pNext = nullptr;
		buffer_info.size = alloc_size;
		buffer_info.usage = usage;

		VmaAllocationCreateInfo vma_alloc_info{};
		vma_alloc_info.usage = memory_usage;
		vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

		allocated_buffer new_buffer{};
		const VkResult result = vmaCreateBuffer(renderer_->opt_allocator.value(), &buffer_info, &vma_alloc_info, &new_buffer.buffer,
			&new_buffer.allocation,
			&new_buffer.info);
		if (result != VK_SUCCESS) return { .result = result };
		{
			const VkDebugUtilsObjectNameInfoEXT buffer_name =  rhi_helpers::add_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(new_buffer.buffer), name);
			if (const VkResult debug_name_result = vkSetDebugUtilsObjectNameEXT(renderer_->opt_device.value(), &buffer_name); debug_name_result != VK_SUCCESS) return  { .result = result };
		}
		return {
			.result = VK_SUCCESS,
			.buffer = new_buffer,
		};
}

void rhi_data::destroy_buffer(const allocated_buffer& buffer) const
{
	vmaDestroyBuffer(renderer_->opt_allocator.value(), buffer.buffer, buffer.allocation);
}

allocated_image_result rhi_data::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
	bool mip_mapped) const
{
	allocated_image new_image{};
	new_image.image_format = format;
	new_image.image_extent = size;

	VkImageCreateInfo img_info = rhi_helpers::img_create_info(format, usage, size);
	if (mip_mapped)
	{
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (VkResult result = vmaCreateImage(renderer_->opt_allocator.value(), &img_info, &alloc_info, &new_image.image,
		&new_image.allocation, nullptr); result != VK_SUCCESS)
	{
		allocated_image_result rv{};
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

	if (VkResult result = vkCreateImageView(renderer_->opt_device.value(), &view_info, nullptr, &new_image.image_view); result !=
		VK_SUCCESS)
	{
		allocated_image_result rv{};
		rv.result = result;
		return rv;
	}
	{
		allocated_image_result rv{};
		rv.result = VK_SUCCESS;
		rv.image = new_image;
		return rv;
	}
}

std::expected<ktxVulkanTexture, ktx_error_code_e> rhi_data::create_image(ktxTexture* ktx_texture, const VkImageUsageFlags usage) const
{
	ktxVulkanTexture texture{};
	ktx_error_code_e ktx_result = ktxTexture_VkUploadEx(ktx_texture, &renderer_->vdi.value(), &texture,
		VK_IMAGE_TILING_OPTIMAL,
		usage,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	if (ktx_result != KTX_SUCCESS)
	{
		return std::unexpected<ktx_error_code_e>(ktx_result);
	}
	return texture;
}


allocated_image_result rhi_data::create_image(const void* data, const VkExtent3D size, const VkFormat format,
                                              const VkImageUsageFlags usage, const bool mip_mapped) const
{
	const size_t data_size = static_cast<size_t>(size.depth) * size.width * size.height * 4;
	auto [result, created_buffer] = create_buffer("image staging", data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
	if (result != VK_SUCCESS)
	{
		allocated_image_result rv{};
		rv.result = result;
		return rv;
	}
	const allocated_buffer staging = created_buffer;

	void* staging_data = nullptr;
	vmaMapMemory(renderer_->opt_allocator.value(), staging.allocation, &staging_data);
	memcpy(static_cast<char*>(staging_data), data, data_size);
	vmaUnmapMemory(renderer_->opt_allocator.value(), staging.allocation);

	const auto image_result = create_image(size, format,
		usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		mip_mapped);
	if (image_result.result != VK_SUCCESS)
	{
		return image_result;
	}
	const allocated_image new_image = image_result.image;

	renderer_->immediate_submit([&](const VkCommandBuffer cmd)
		{
			rhi::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			VkBufferImageCopy copy_region{};
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

			rhi::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});

	destroy_buffer(staging);
	{
		allocated_image_result rv = {};
		rv.result = VK_SUCCESS;
		rv.image = new_image;
		return rv;
	}
}

void rhi_data::destroy_image(const allocated_image& img) const
{
	vkDestroyImageView(renderer_->opt_device.value(), img.image_view, nullptr);
	vmaDestroyImage(renderer_->opt_allocator.value(), img.image, img.allocation);
}
