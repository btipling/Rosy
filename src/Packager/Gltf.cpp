#include "Gltf.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <iostream>

using namespace rosy_packager;

rosy::result gltf::import()
{
	std::filesystem::path file_path{ source_path };
	constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember |
		fastgltf::Options::AllowDouble |
		fastgltf::Options::LoadExternalBuffers |
		fastgltf::Options::DecomposeNodeMatrices;
	fastgltf::Asset gltf;
	fastgltf::Parser parser{};
	auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
	if (data.error() != fastgltf::Error::None) {
		auto err = fastgltf::to_underlying(data.error());
		std::cerr << std::format("import - failed to open {}, {}", source_path, err) << '\n';
		return rosy::result::error;
	}
	auto asset = parser.loadGltf(data.get(), file_path.parent_path(), gltf_options);
	if (asset) {
		gltf = std::move(asset.get());
	}
	else {
		auto err = fastgltf::to_underlying(asset.error());
		std::cerr << std::format("import - failed to load {}, {}", source_path, err) << '\n';
		return rosy::result::error;
	}
	gltf_asset.positions.clear();
	gltf_asset.indices.clear();

	for (fastgltf::Mesh& mesh : gltf.meshes) {
		for (size_t i = 0; i < mesh.primitives.size(); i++) {  // NOLINT(modernize-loop-convert) - mesh.primitives RAII issues.
			{
				const auto position_it = mesh.primitives[i].findAttribute("POSITION");
				auto& pos_accessor = gltf.accessors[position_it->accessorIndex];

				uint32_t initial_vtx = static_cast<uint32_t>(gltf_asset.positions.size());
				gltf_asset.positions.resize(gltf_asset.positions.size() + pos_accessor.count);

				{
					fastgltf::Accessor& index_accessor = gltf.accessors[mesh.primitives[i].indicesAccessor.value()];
					gltf_asset.indices.reserve(gltf_asset.indices.size() + index_accessor.count);

					fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor,
						[&](const std::uint32_t idx) {
							gltf_asset.indices.push_back(idx + initial_vtx);
						});
				}

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, pos_accessor,
					[&](const fastgltf::math::fvec3& v, const size_t index) {
						position new_position{};
						new_position.vertex = { v[0], v[1], v[2] };
						new_position.normal = { 1.0f, 0.0f, 0.0f };
						gltf_asset.positions[initial_vtx + index] = new_position;
					});

				if (auto normals = mesh.primitives[i].findAttribute("NORMAL"); normals != mesh.primitives[i].attributes.end()) {
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex],
						[&](const fastgltf::math::fvec3 n, const size_t index) {
							gltf_asset.positions[initial_vtx + index].normal = { n[0], n[1], n[2] };
						});
				}

			}
		}
	}
	return rosy::result::ok;
}
