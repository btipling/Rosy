#pragma once
#include "../rhi/RHI.h"
#include "../utils/utils.h"
#include <unordered_map>
#include <filesystem>

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

class Rhi;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Rhi* rhi, std::filesystem::path filePath);