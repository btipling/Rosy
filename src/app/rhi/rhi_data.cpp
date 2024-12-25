#include "rhi.h"
#include "../rhi/rhi_types.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <iostream>
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <stb_image.h>

texture_id texture_cache::add_texture(const VkImageView& image, const VkSampler sampler)
{
	for (unsigned int i = 0; i < cache.size(); i++) {
		if (cache[i].imageView == image && cache[i].sampler == sampler) {
			return texture_id{ i };
		}
	}
	const uint32_t idx = cache.size();
	cache.push_back(VkDescriptorImageInfo{ .sampler = sampler,.imageView = image, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
	return texture_id{ idx };
}

rhi_data::rhi_data(rhi* renderer) : renderer_{ renderer }
{
	ktx_sub_allocator::init_vma(renderer->opt_allocator.value());
	sub_allocator_callbacks_ = {
		ktx_sub_allocator::alloc_mem_c_wrapper,
		ktx_sub_allocator::bind_buffer_memory_c_wrapper,
		ktx_sub_allocator::bind_image_memory_c_wrapper,
		ktx_sub_allocator::map_memory_c_wrapper,
		ktx_sub_allocator::unmap_memory_c_wrapper,
		ktx_sub_allocator::free_mem_c_wrapper
	};
}
namespace {
	VkFilter extract_filter(const fastgltf::Filter filter)
	{
		switch (filter) {
		case fastgltf::Filter::Nearest:
		case fastgltf::Filter::NearestMipMapNearest:
		case fastgltf::Filter::NearestMipMapLinear:
			return VK_FILTER_NEAREST;
		case fastgltf::Filter::Linear:
		case fastgltf::Filter::LinearMipMapNearest:
		case fastgltf::Filter::LinearMipMapLinear:
		default:
			return VK_FILTER_LINEAR;
		}
	}

	VkSamplerMipmapMode extract_mipmap_mode(const fastgltf::Filter filter)
	{
		switch (filter) {
		case fastgltf::Filter::NearestMipMapNearest:
		case fastgltf::Filter::LinearMipMapNearest:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;

		case fastgltf::Filter::NearestMipMapLinear:
		case fastgltf::Filter::LinearMipMapLinear:
		default:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
	}

	VkSamplerAddressMode extract_wrap_mode(const fastgltf::Wrap wrap)
	{
		switch (wrap) {
		case fastgltf::Wrap::ClampToEdge:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case fastgltf::Wrap::MirroredRepeat:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case fastgltf::Wrap::Repeat:
		default:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}
}

rh::result rhi_data::load_gltf_meshes(const rh::ctx& ctx, std::filesystem::path file_path, mesh_scene& gltf_mesh_scene)
{
	constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember |
		fastgltf::Options::AllowDouble |
		fastgltf::Options::LoadExternalBuffers |
		fastgltf::Options::DecomposeNodeMatrices;
	fastgltf::Asset gltf;
	fastgltf::Parser parser{};
	auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
	if (data.error() != fastgltf::Error::None) {
		return rh::result::error;
	}
	auto asset = parser.loadGltf(data.get(), file_path.parent_path(), gltf_options);
	if (asset) {
		gltf = std::move(asset.get());
	}
	else {
		auto err = fastgltf::to_underlying(asset.error());
		rosy_utils::debug_print_a("failed to load gltf: %d %s\n", err, file_path.string().c_str());
		return rh::result::error;
	}

	std::vector<VkSamplerCreateInfo> sampler_create_infos{};

	for (fastgltf::Image& image : gltf.images) {
		if (std::expected<ktx_auto_texture, ktx_error_code_e> res = create_image(gltf, image, VK_FORMAT_R8G8B8A8_SRGB); res.has_value()) {
			auto ktx_txt = res.value();
			auto [ktx_texture, ktx_vk_texture] = ktx_txt;
			gltf_mesh_scene.ktx_textures.push_back(ktx_txt);

			VkImageView img_view{};
			{
				ktxTexture* k_texture;
				ktx_error_code_e ktx_result = ktxTexture_CreateFromNamedFile("assets/skybox_clouds.ktx2",
					KTX_TEXTURE_CREATE_NO_FLAGS,
					&k_texture);
				if (ktx_result != KTX_SUCCESS) {
					rosy_utils::debug_print_a("ktx read failure: %d\n", ktx_result);
					continue;
				}
				VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
				VkImageViewCreateInfo view_info = rhi_helpers::img_view_create_info(ktx_vk_texture.imageFormat, ktx_vk_texture.image, aspect_flag);
				view_info.subresourceRange.levelCount = ktx_vk_texture.levelCount;
				view_info.subresourceRange.layerCount = ktx_vk_texture.layerCount;
				view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				if (VkResult result = vkCreateImageView(renderer_->opt_device.value(), &view_info, nullptr, &img_view); result !=
					VK_SUCCESS)
				{
					continue;
				}
				gltf_mesh_scene.image_views.push_back(img_view);
			}
		}
		else {
			rosy_utils::debug_print_a("failed to create gltf texture image: %d\n", res.error());
		}
	}

	for (fastgltf::Sampler& sampler : gltf.samplers) {
		VkSamplerCreateInfo sampler_create_info = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
		sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
		sampler_create_info.minLod = 0;
		sampler_create_info.addressModeU = extract_wrap_mode(sampler.wrapS);
		sampler_create_info.addressModeV = extract_wrap_mode(sampler.wrapT);
		sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		sampler_create_info.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
		sampler_create_info.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

		sampler_create_infos.push_back(sampler_create_info);

	}
	{
		std::vector<material_data> materials;
		size_t desc_index{ 0 };
		for (fastgltf::Material& mat : gltf.materials) {
			material_data m_data{};
			material m{};
			if (mat.pbrData.baseColorTexture.has_value()) {
				auto image_index = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				auto sampler_index = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
				VkSamplerCreateInfo sample = sampler_create_infos[sampler_index];
				auto [texture, vk_texture] = gltf_mesh_scene.ktx_textures[image_index];
				auto img_view = gltf_mesh_scene.image_views[image_index];
				VkSampler sampler{};
				{
					sample.maxLod = vk_texture.levelCount;
					sample.minLod = 0;

					sample.anisotropyEnable = VK_FALSE;
					sample.maxAnisotropy = 0.f;
					sample.mipLodBias = 0.0f;

					sample.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
					sampler_create_infos.push_back(sample);
					if (VkResult result = vkCreateSampler(renderer_->opt_device.value(), &sample, nullptr, &sampler); result != VK_SUCCESS) continue;
					gltf_mesh_scene.samplers.push_back(sampler);
				}
				{
					auto [image_set_result, image_set] = gltf_mesh_scene.descriptor_allocator.value().allocate(renderer_->opt_device.value(), gltf_mesh_scene.image_layout);
					if (image_set_result != VK_SUCCESS) continue;
					{
						descriptor_writer writer;
						writer.write_sampled_image(0, img_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
						writer.write_sampler(1, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLER);
						writer.update_set(renderer_->opt_device.value(), image_set);
						gltf_mesh_scene.descriptor_sets.push_back(image_set);
						m_data.color_texture_index = 0;
						m_data.color_sampler_index = 0;
					}
				}
				m.descriptor_set_id = desc_index;
				desc_index++;
			}
			materials.push_back(m_data);
			gltf_mesh_scene.materials.push_back(m);
		}

		auto [material_create_result, created_material_buffers] = upload_materials(materials);
		if (material_create_result != VK_SUCCESS) {
			rosy_utils::debug_print_a("failed to create materials buffer: %d\n", material_create_result);
			return rh::result::error;
		}
		gltf_mesh_scene.material_buffers = created_material_buffers;
	}

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
						new_vtx.texture_coordinates_x = 0.0f;
						new_vtx.texture_coordinates_y = 0.0f;
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
						vertices[initial_vtx + index].texture_coordinates_x = tc.x;
						vertices[initial_vtx + index].texture_coordinates_y = tc.y;
					});
			}

			if (auto colors = p.findAttribute("COLOR_0"); colors != p.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
					[&](const glm::vec4 v, const size_t index) {
						vertices[initial_vtx + index].color = v;
					});
			}

			if (p.materialIndex.has_value()) {
				new_surface.material = p.materialIndex.value();
			}
			else {
				new_surface.material = 0;
			}

			new_mesh.surfaces.push_back(new_surface);
		}

		if (constexpr bool override_colors = true) {
			for (vertex& vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.0);
			}
		}
		auto [mesh_result, uploaded_mesh] = upload_mesh(indices, vertices);
		if (mesh_result != VK_SUCCESS) {
			rosy_utils::debug_print_a("failed to upload mesh: %d\n", mesh_result);
			return rh::result::error;
		}
		new_mesh.mesh_buffers = uploaded_mesh;
		gltf_mesh_scene.meshes.emplace_back(std::make_shared<mesh_asset>(std::move(new_mesh)));
	}
	for (fastgltf::Node& gltf_node : gltf.nodes) gltf_mesh_scene.add_node(gltf_node);
	for (fastgltf::Scene& gltf_scene : gltf.scenes) gltf_mesh_scene.add_scene(gltf_scene);
	gltf_mesh_scene.root_scene = gltf.defaultScene.value_or(0);

	{
		// Need to count the primitives correctly for the scene to size the render buffer correctly.
		size_t num_surfaces{ 0 };
		std::queue<std::shared_ptr<mesh_node>> queue{};
		for (const size_t node_index : gltf_mesh_scene.scenes[gltf_mesh_scene.root_scene]) queue.push(gltf_mesh_scene.nodes[node_index]);
		size_t mesh_index = 0;
		while (queue.size() > 0)
		{
			const auto current_node = queue.front();
			queue.pop();
			if (current_node->mesh_index.has_value())
			{
				const std::shared_ptr<mesh_asset> ma = gltf_mesh_scene.meshes[current_node->mesh_index.value()];
				num_surfaces += ma->surfaces.size(); // All this just to count surfaces correctly.
			}
			for (const size_t child_index : current_node->children) queue.push(gltf_mesh_scene.nodes[child_index]);
		}

		auto [render_create_result, created_render_buffers] = create_render_data(num_surfaces);
		if (render_create_result != VK_SUCCESS) {
			rosy_utils::debug_print_a("failed to create render buffer: %d\n", render_create_result);
			return rh::result::error;
		}
		gltf_mesh_scene.render_buffers = created_render_buffers;
	}

	return rh::result::ok;
}

gpu_render_buffers_result rhi_data::create_render_data(const size_t num_surfaces) const
{
	const size_t render_buffer_size = num_surfaces * sizeof(render_data);

	auto [create_result, new_render_buffer] = create_buffer(
		"renderBuffer",
		render_buffer_size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	if (create_result != VK_SUCCESS) {
		gpu_render_buffers_result fail{};
		fail.result = create_result;
		return fail;
	}

	const allocated_buffer render_buffer = new_render_buffer;

	VkBufferDeviceAddressInfo device_address_info{};
	device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	device_address_info.buffer = render_buffer.buffer;

	const VkDeviceAddress render_buffer_address = vkGetBufferDeviceAddress(renderer_->opt_device.value(), &device_address_info);

	gpu_render_buffers grb{};
	grb.render_buffer = render_buffer;
	grb.render_buffer_address = render_buffer_address;
	grb.buffer_size = render_buffer_size;
	gpu_render_buffers_result rv{};
	rv.result = VK_SUCCESS;
	rv.render_buffers = grb;
	return rv;
}

auto rhi_data::create_scene_data() const -> gpu_scene_buffers_result
{
	constexpr size_t render_buffer_size = sizeof(gpu_scene_data);

	auto [create_result, new_scene_buffer] = create_buffer(
		"sceneBuffer",
		render_buffer_size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO);
	if (create_result != VK_SUCCESS) {
		gpu_scene_buffers_result fail{};
		fail.result = create_result;
		return fail;
	}

	const allocated_buffer scene_buffer = new_scene_buffer;

	VkBufferDeviceAddressInfo device_address_info{};
	device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	device_address_info.buffer = scene_buffer.buffer;

	const VkDeviceAddress scene_buffer_address = vkGetBufferDeviceAddress(renderer_->opt_device.value(), &device_address_info);

	gpu_scene_buffers gsb{};
	gsb.scene_buffer = scene_buffer;
	gsb.scene_buffer_address = scene_buffer_address;
	gsb.buffer_size = render_buffer_size;
	gpu_scene_buffers_result rv{};
	rv.result = VK_SUCCESS;
	rv.scene_buffers = gsb;
	return rv;
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

	VkBufferDeviceAddressInfo device_address_info{};
	device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	device_address_info.buffer = vertex_buffer.buffer;

	// *** SETTING VERTEX BUFFER ADDRESS *** //
	vertex_buffer_address = vkGetBufferDeviceAddress(renderer_->opt_device.value(), &device_address_info);

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

	auto [result, new_staging_buffer] = create_buffer("staging", vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (result != VK_SUCCESS) {
		gpu_mesh_buffers_result fail{};
		fail.result = result;
		return fail;
	}
	allocated_buffer staging = new_staging_buffer;

	void* data;
	vmaMapMemory(renderer_->opt_allocator.value(), staging.allocation, &data);
	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy(static_cast<char*>(data) + vertex_buffer_size, indices.data(), index_buffer_size);
	vmaUnmapMemory(renderer_->opt_allocator.value(), staging.allocation);

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

	gpu_mesh_buffers buffers{};
	buffers.vertex_buffer = vertex_buffer;
	buffers.vertex_buffer_address = vertex_buffer_address;
	buffers.index_buffer = index_buffer;

	gpu_mesh_buffers_result rv{};
	rv.result = VK_SUCCESS;
	rv.buffers = buffers;

	return rv;
}

auto rhi_data::upload_materials(std::span<material_data> materials) const -> gpu_material_buffers_result
{
	allocated_buffer material_buffer;
	VkDeviceAddress material_buffer_address;

	const size_t material_buffer_size = materials.size() * sizeof(material_data);

	auto [material_result, new_material_buffer] = create_buffer(
		"materialBuffer",
		material_buffer_size,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	if (material_result != VK_SUCCESS) {
		gpu_material_buffers_result fail{};
		fail.result = material_result;
		return fail;
	}

	// *** SETTING MATERIAL BUFFER *** //
	material_buffer = new_material_buffer;

	VkBufferDeviceAddressInfo device_address_info{};
	device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	device_address_info.buffer = material_buffer.buffer;

	// *** SETTING MATERIAL BUFFER ADDRESS *** //
	material_buffer_address = vkGetBufferDeviceAddress(renderer_->opt_device.value(), &device_address_info);


	auto [result, new_staging_buffer] = create_buffer("staging", material_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	if (result != VK_SUCCESS) {
		gpu_material_buffers_result fail{};
		fail.result = result;
		return fail;
	}
	allocated_buffer staging = new_staging_buffer;

	void* data;
	vmaMapMemory(renderer_->opt_allocator.value(), staging.allocation, &data);
	memcpy(data, materials.data(), material_buffer_size);
	vmaUnmapMemory(renderer_->opt_allocator.value(), staging.allocation);

	VkResult submit_result;
	submit_result = renderer_->immediate_submit([&](const VkCommandBuffer cmd) {
		VkBufferCopy vertex_copy{ 0 };
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = material_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, material_buffer.buffer, 1, &vertex_copy);
		});
	if (submit_result != VK_SUCCESS) {
		gpu_material_buffers_result fail{};
		fail.result = submit_result;
		return fail;
	}
	destroy_buffer(staging);

	gpu_material_buffers buffers{};
	buffers.material_buffer = material_buffer;
	buffers.material_buffer_address = material_buffer_address;

	gpu_material_buffers_result rv{};
	rv.result = VK_SUCCESS;
	rv.buffers = buffers;

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
		const VkDebugUtilsObjectNameInfoEXT buffer_name = rhi_helpers::add_name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(new_buffer.buffer), name);
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

std::expected<ktxVulkanTexture, ktx_error_code_e> rhi_data::create_image(ktxTexture* ktx_texture, const VkImageUsageFlags usage)
{
	ktxVulkanTexture texture{};
	ktx_error_code_e ktx_result = ktxTexture_VkUploadEx_WithSuballocator(ktx_texture, &renderer_->vdi.value(), &texture,
		VK_IMAGE_TILING_OPTIMAL,
		usage,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		&sub_allocator_callbacks_);
	if (ktx_result != KTX_SUCCESS)
	{
		return std::unexpected<ktx_error_code_e>(ktx_result);
	}
	return texture;
}


std::expected<ktx_auto_texture, ktx_error_code_e> rhi_data::create_image(const void* data, const VkExtent3D size, const VkFormat format,
	const VkImageUsageFlags usage, const bool mip_mapped)
{
	const size_t data_size = static_cast<size_t>(size.depth) * size.width * size.height * 4;

	ktxTextureCreateInfo create_info{};
	create_info.vkFormat = static_cast<ktx_uint32_t>(format);
	create_info.baseWidth = size.width;
	create_info.baseHeight = size.height;
	create_info.baseDepth = size.depth;
	create_info.numDimensions = 2;
	create_info.numLevels = 1;
	create_info.numLayers = 1;
	create_info.numFaces = 1;
	create_info.pDfd = nullptr;
	create_info.isArray = false;
	create_info.generateMipmaps = KTX_TRUE;

	ktxTexture2* texture;
	KTX_error_code result = ktxTexture2_Create(
		&create_info,
		KTX_TEXTURE_CREATE_ALLOC_STORAGE,
		&texture
	);
	if (result != KTX_SUCCESS) {
		return std::unexpected(result);
	}
	result = ktxTexture_SetImageFromMemory(
		ktxTexture(texture),
		0,
		0,
		0,
		static_cast<const ktx_uint8_t*>(data),
		data_size
	);
	if (result != KTX_SUCCESS) {
		ktxTexture_Destroy(ktxTexture(texture));
		return std::unexpected(result);
	}
	if (auto res = create_image(ktxTexture(texture), VK_IMAGE_USAGE_SAMPLED_BIT); res.has_value())
	{
		ktx_auto_texture rv{};
		rv.vk_texture = res.value();
		rv.texture = ktxTexture(texture);
		return rv;
	}
	else return std::unexpected(res.error());
}

std::expected<ktx_auto_texture, ktx_error_code_e> rhi_data::create_image(fastgltf::Asset& asset,
	const fastgltf::Image& image, const VkFormat format)
{
	int width, height, num_channels;
	if (const fastgltf::sources::BufferView* view = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
		auto& [
			bufferIndex,
				byteOffset,
				byteLength,
				byteStride,
				target,
				// ReSharper disable once IdentifierTypo
				meshoptCompression,
				name
		] = asset.bufferViews[view->bufferViewIndex];
		const auto& buffer = asset.buffers[bufferIndex];
		if (const fastgltf::sources::Vector* vector = std::get_if<fastgltf::sources::Vector>(&buffer.data)) {
			unsigned char* data = stbi_load_from_memory(static_cast<const stbi_uc*>(static_cast<const void*>(vector->bytes.data())) + byteOffset, // NOLINT(bugprone-casting-through-void)
				static_cast<int>(byteLength),
				&width, &height, &num_channels, 4);

			if (data) {
				VkExtent3D image_size{};
				image_size.width = width;
				image_size.height = height;
				image_size.depth = 1;
				std::expected<ktx_auto_texture, ktx_error_code_e> rv = create_image(data, image_size, format, VK_IMAGE_USAGE_SAMPLED_BIT, false);

				stbi_image_free(data);
				return rv;
			}
		}
		if (const fastgltf::sources::Array* arr = std::get_if<fastgltf::sources::Array>(&buffer.data)) {
			unsigned char* data = stbi_load_from_memory(static_cast<const stbi_uc*>(static_cast<const void*>(arr->bytes.data())) + byteOffset, // NOLINT(bugprone-casting-through-void)
				static_cast<int>(byteLength),
				&width, &height, &num_channels, 4);

			if (data) {
				VkExtent3D image_size{};
				image_size.width = width;
				image_size.height = height;
				image_size.depth = 1;

				std::expected<ktx_auto_texture, ktx_error_code_e> rv = create_image(data, image_size, format, VK_IMAGE_USAGE_SAMPLED_BIT, false);

				stbi_image_free(data);
				return rv;
			}
		}
	}

	return std::unexpected(KTX_FILE_DATA_ERROR);
}

void rhi_data::destroy_image(const allocated_image& img) const
{
	vkDestroyImageView(renderer_->opt_device.value(), img.image_view, nullptr);
	vmaDestroyImage(renderer_->opt_allocator.value(), img.image, img.allocation);
}

void rhi_data::destroy_image(ktx_auto_texture& img)
{
	ktxTexture_Destroy(img.texture);
	ktxVulkanTexture_Destruct_WithSuballocator(&img.vk_texture, renderer_->opt_device.value(), nullptr, &sub_allocator_callbacks_);
}