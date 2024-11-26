#include "../utils/utils.h"
#include <unordered_map>
#include <filesystem>
#include "../Rosy.h"

class rhi;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(rhi* rhi, std::filesystem::path file_path);