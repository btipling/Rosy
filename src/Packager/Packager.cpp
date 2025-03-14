#include "pch.h"
#include "Packager.h"

#include <meshoptimizer.h>

using namespace rosy_packager;

void rosy_packager::optimize_mesh(const std::shared_ptr<rosy_logger::log>& l, rosy_asset::mesh& asset_mesh)
{
    size_t total_vertices = asset_mesh.positions.size();
    const size_t total_indices = asset_mesh.indices.size();
    l->info(std::format("optimize-mesh: starting vertices count: {}", total_vertices));
    std::vector<unsigned int> remap(total_indices);
    size_t new_vertices_count = meshopt_generateVertexRemap(remap.data(), asset_mesh.indices.data(), total_indices, asset_mesh.positions.data(), total_vertices, sizeof(rosy_asset::position));
    l->info(std::format("optimize-mesh: new vertices count: {}", new_vertices_count));

    std::vector<uint32_t> optimized_indices(total_indices);
    meshopt_remapIndexBuffer(optimized_indices.data(), asset_mesh.indices.data(), total_indices, remap.data());
    asset_mesh.indices = std::move(optimized_indices);

    std::vector<rosy_asset::position> optimized_positions(new_vertices_count);
    meshopt_remapVertexBuffer(optimized_positions.data(), asset_mesh.positions.data(), total_vertices, sizeof(rosy_asset::position), remap.data());
    asset_mesh.positions = std::move(optimized_positions);
}
