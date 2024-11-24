#include "../utils/utils.h"
#include <unordered_map>
#include <filesystem>
#include "../Rosy.h"

class Rhi;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Rhi* rhi, std::filesystem::path filePath);