#include "stb_image.h"
#include <iostream>
#include "loader.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "../rhi/RHI.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>


std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(Rhi* rhi, std::filesystem::path file_path) {
	fastgltf::Asset gltf;
	fastgltf::Parser parser{};
    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
    if (data.error() != fastgltf::Error::None) {
        return std::nullopt;
    }
    auto asset = parser.loadGltf(data.get(), file_path.parent_path(), fastgltf::Options::None);
	if (asset) {
		gltf = std::move(asset.get());
	} else {
        auto err = fastgltf::to_underlying(asset.error());
		rosy_utils::debug_print_a("failed to load gltf: %d %s\n", err, file_path.string().c_str());
		return std::nullopt;
	}
    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // use the same vectors for all meshes so that the memory doesn't reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshAsset new_mesh;

        new_mesh.name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (fastgltf::Primitive p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
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
                        Vertex newvtx;
                        newvtx.position = glm::vec4{ v, 1.0f };
                        newvtx.normal = { 1.0f, 0.0f, 0.0f, 1.0f };
                        newvtx.color = glm::vec4{ 1.f };
                        newvtx.textureCoordinates = { 0.0f, 0.0f, 0.0f, 0.0f };
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
                        vertices[initial_vtx + index].textureCoordinates = { v.x, v.y, 0.0f, 0.0f };
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
        constexpr bool OverrideColors = true;
        if (OverrideColors) {
            for (Vertex& vtx : vertices) {
                vtx.color = vtx.normal;
            }
        }
        auto result = rhi->upload_mesh(indices, vertices);
        if (result.result != VK_SUCCESS) {
            rosy_utils::debug_print_a("failed to upload mesh: %d\n", result.result);
            return std::nullopt;
        }
        new_mesh.meshBuffers = result.buffers;
        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(new_mesh)));
    }

    return meshes;
}