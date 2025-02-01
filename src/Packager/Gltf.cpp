#include "Gltf.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <iostream>

using namespace rosy_packager;

namespace
{
	uint16_t filter_to_val(const fastgltf::Filter filter)
	{
		switch (filter)
		{
		case fastgltf::Filter::Nearest:
			return 0;
		case fastgltf::Filter::NearestMipMapNearest:
			return 1;
		case fastgltf::Filter::LinearMipMapNearest:
			return 2;
		case fastgltf::Filter::Linear:
			return 3;
		case fastgltf::Filter::NearestMipMapLinear:
			return 4;
		case fastgltf::Filter::LinearMipMapLinear:
			return 5;
		}
		return UINT16_MAX;
	}

	uint16_t wrap_to_val(const fastgltf::Wrap wrap)
	{
		switch (wrap)
		{
		case fastgltf::Wrap::ClampToEdge:
			return 0;
		case fastgltf::Wrap::MirroredRepeat:
			return 1;
		case fastgltf::Wrap::Repeat:
			return 2;
		}
		return UINT16_MAX;
	}

}

rosy::result gltf::import(rosy::log* l)
{

	const std::filesystem::path file_path{ source_path };

	constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::DecomposeNodeMatrices;

	fastgltf::Asset gltf;
	fastgltf::Parser parser{};
	auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
	if (data.error() != fastgltf::Error::None) {
		auto err = fastgltf::to_underlying(data.error());
		l->error(std::format("import - failed to open {}, {}", source_path, err));
		return rosy::result::error;
	}
	auto asset = parser.loadGltf(data.get(), file_path.parent_path(), gltf_options);
	if (asset) {
		gltf = std::move(asset.get());
	}
	else {
		auto err = fastgltf::to_underlying(asset.error());
		l->error(std::format("import - failed to load {}, {}", source_path, err));
		return rosy::result::error;
	}

	// IMAGES
	for (auto& [gltf_image_data, gltf_image_name] : gltf.images) {
		l->debug(std::format("Adding image: {}", gltf_image_name));
		image img{};
		std::ranges::copy(gltf_image_name, std::back_inserter(img.name));
		gltf_asset.images.push_back(img);
	}

	// MATERIALS

	{
		for (fastgltf::Material& mat : gltf.materials) {
			material m{};
			if (mat.pbrData.baseColorTexture.has_value()) {
				if (gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.has_value())
				{
					m.color_image_index = static_cast<uint32_t>(gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value());
				}
				else
				{
					m.color_image_index = UINT32_MAX;
				}
				if (gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.has_value())
				{
					m.color_sampler_index = static_cast<uint32_t>(gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value());
				}
				else
				{
					m.color_sampler_index = UINT32_MAX;
				}
			}
			if (mat.normalTexture.has_value()) {
				if (gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.has_value())
				{
					m.normal_image_index = static_cast<uint32_t>(gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value());
				}
				else
				{
					m.normal_image_index = UINT32_MAX;
				}
				if (gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.has_value())
				{
					m.normal_sampler_index = static_cast<uint32_t>(gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value());
				}
				else
				{
					m.normal_sampler_index = UINT32_MAX;
				}
			}
			else
			{
				m.normal_image_index = UINT32_MAX;
				m.normal_sampler_index = UINT32_MAX;
			}
			{
				fastgltf::math::nvec4 c = mat.pbrData.baseColorFactor;
				m.base_color_factor = { c[0], c[1], c[2], c[3] };
				m.double_sided = mat.doubleSided;
				m.metallic_factor = mat.pbrData.metallicFactor;
				m.roughness_factor = mat.pbrData.roughnessFactor;
				m.alpha_mode = static_cast<uint8_t>(mat.alphaMode);
				m.alpha_cutoff = mat.alphaCutoff;
			}
			gltf_asset.materials.push_back(m);
		}
	}

	// SAMPLERS

	{
		for (fastgltf::Sampler& gltf_sampler : gltf.samplers) {
			sampler smp{};
			if (gltf_sampler.magFilter.has_value())
			{
				smp.mag_filter = filter_to_val(gltf_sampler.magFilter.value());
			}
			else
			{
				smp.mag_filter = UINT16_MAX;
			}
			if (gltf_sampler.minFilter.has_value())
			{
				smp.min_filter = filter_to_val(gltf_sampler.minFilter.value());
			}
			else
			{
				smp.min_filter = UINT16_MAX;
			}
			smp.wrap_s = wrap_to_val(gltf_sampler.wrapS);
			smp.wrap_t = wrap_to_val(gltf_sampler.wrapT);
			gltf_asset.samplers.push_back(smp);
		}
	}

	// MESHES


	for (fastgltf::Mesh& fast_gltf_mesh : gltf.meshes) {
		mesh new_mesh{};
		for (auto& primitive : fast_gltf_mesh.primitives)
		{
			// PRIMITIVE SURFACE
			surface new_surface{};
			new_surface.start_index = static_cast<uint32_t>(new_mesh.indices.size());
			new_surface.count = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count);

			const auto position_it = primitive.findAttribute("POSITION");
			auto& pos_accessor = gltf.accessors[position_it->accessorIndex];

			uint32_t initial_vtx = static_cast<uint32_t>(new_mesh.positions.size());
			new_mesh.positions.resize(new_mesh.positions.size() + pos_accessor.count);

			// PRIMITIVE INDEX
			{
				std::array<size_t, 3> triangle{};
				int current_index{ 0 };
				fastgltf::Accessor& index_accessor = gltf.accessors[primitive.indicesAccessor.value()];
				new_mesh.indices.reserve(new_mesh.indices.size() + index_accessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor,
					[&](const std::uint32_t idx) {
						new_mesh.indices.push_back(idx + initial_vtx);
						{
							// Track triangles for tangent calculations later.
							triangle[current_index] = idx;
							current_index += 1;
							if (current_index == 3)
							{
								triangle = { 0, 0, 0 };
								current_index = 0;
							}
						}
					});
			}

			// PRIMITIVE VERTEX
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, pos_accessor,
				[&](const fastgltf::math::fvec3& v, const size_t index) {
					position new_position{};
					new_position.vertex = { v[0], v[1], v[2] };
					new_position.normal = { 1.0f, 0.0f, 0.0f };
					new_position.tangents = { 1.0f, 0.0f, 0.0f };
					new_mesh.positions[initial_vtx + index] = new_position;
				});

			// PRIMITIVE NORMAL
			if (const auto normals = primitive.findAttribute("NORMAL"); normals != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex],
					[&](const fastgltf::math::fvec3& n, const size_t index) {
						new_mesh.positions[initial_vtx + index].normal = { n[0], n[1], n[2] };
					});
			}

			// ReSharper disable once StringLiteralTypo
			if (auto uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end()) {

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uv->accessorIndex],
					[&](const fastgltf::math::fvec2& tc, const size_t index) {
						new_mesh.positions[initial_vtx + index].texture_coordinates = { tc[0], tc[1] };
					});
			}

			// PRIMITIVE COLOR
			if (auto colors = primitive.findAttribute("COLOR_0"); colors != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex],
					[&](const fastgltf::math::fvec4& c, const size_t index) {
						new_mesh.positions[initial_vtx + index].color = { c[0], c[1], c[2], c[3] };
					});
			}

			// PRIMITIVE TANGENT
			if (auto tangents = primitive.findAttribute("TANGENT"); tangents != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[tangents->accessorIndex],
					[&](const fastgltf::math::fvec4& t, const size_t index) {
						new_mesh.positions[initial_vtx + index].tangents = { t[0], t[1], t[2], t[3] };
					});
			}

			// PRIMITIVE MATERIAL
			if (primitive.materialIndex.has_value()) {
				new_surface.material = static_cast<uint32_t>(primitive.materialIndex.value());
			}
			else {
				new_surface.material = 0;
			}
			new_mesh.surfaces.push_back(new_surface);
		}
		gltf_asset.meshes.push_back(new_mesh);
	}

	gltf_asset.scenes.reserve(gltf.scenes.size());
	for (auto& [nodeIndices, name] : gltf.scenes)
	{
		scene s{};
		s.nodes.reserve(nodeIndices.size());
		for (size_t node_index : nodeIndices)
		{
			s.nodes.push_back(static_cast<uint32_t>(node_index));
		}
		gltf_asset.scenes.push_back(s);
	}

	gltf_asset.nodes.reserve(gltf.nodes.size());
	for (auto& gltf_node : gltf.nodes)
	{
		node n{};
		n.mesh_id = static_cast<uint32_t>(gltf_node.meshIndex.value_or(SIZE_MAX));

		auto& [tr, ro, sc] = std::get<fastgltf::TRS>(gltf_node.transform);

		const auto tm = translate(fastgltf::math::fmat4x4(1.0f), tr);
		const auto rm = fastgltf::math::fmat4x4(asMatrix(ro));
		const auto sm = scale(fastgltf::math::fmat4x4(1.0f), sc);
		const auto transform = tm * rm * sm;

		for (size_t i{ 0 }; i < 4; i++)
		{
			for (size_t j{ 0 }; j < 4; j++)
			{
				n.transform[i * 4 + j] = transform[i][j];
			}
		}
		n.child_nodes.reserve(gltf_node.children.size());
		for (size_t node_index : gltf_node.children)
		{
			n.child_nodes.push_back(static_cast<uint32_t>(node_index));
		}
		gltf_asset.nodes.push_back(n);
	}
	return rosy::result::ok;
}
