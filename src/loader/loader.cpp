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


std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Rhi* rhi, std::filesystem::path filePath) {
	fastgltf::Asset gltf;
	fastgltf::Parser parser{};
	fastgltf::GltfDataBuffer data;
	data.loadFromFile(filePath);
	auto asset = parser.loadGltf(&data, filePath.parent_path(), fastgltf::Options::None);
	if (asset) {
		gltf = std::move(asset.get());
	} else {
		rosy_utils::DebugPrintA("failed to load gltf: %s\n", fastgltf::to_underlying(asset.error()));
		return {};
	}
    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshAsset newmesh;

        newmesh.name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                    });

            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
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

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = glm::vec4{ v, 0.0f };
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].textureCoordinates = { v.x, v.y, 0.0f, 0.0f };
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }
            newmesh.surfaces.push_back(newSurface);
        }

        // display the vertex normals
        constexpr bool OverrideColors = true;
        if (OverrideColors) {
            for (Vertex& vtx : vertices) {
                vtx.color = vtx.normal;
            }
        }
        auto result = rhi->uploadMesh(indices, vertices);
        if (result.result != VK_SUCCESS) {
            rosy_utils::DebugPrintA("failed to upload mesh: %d\n", result.result);
            return {};
        }
        newmesh.meshBuffers = result.buffers;
        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
    }

    return meshes;
}